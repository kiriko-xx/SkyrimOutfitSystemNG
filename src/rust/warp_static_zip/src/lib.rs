//! A crate for embedding static assets into warp webservers.
#![warn(rust_2018_idioms, missing_docs)]

use headers::{
    AcceptRanges, ContentLength, ContentRange, ContentType, HeaderMapExt, IfModifiedSince, IfRange,
    IfUnmodifiedSince, LastModified, Range,
};
use http::{HeaderMap, StatusCode};
use hyper::Body;
use std::future::ready;
use std::io::{Read, Seek};
use std::{path::PathBuf, time::SystemTime};
use warp::{
    filters::header::headers_cloned,
    path::{tail, Tail},
    reject::{self, Rejection},
    reply::Response,
    Filter,
};

#[doc(hidden)]
pub use zip::ZipArchive;

#[doc(hidden)]
pub use once_cell::sync::Lazy;

#[doc(hidden)]
pub fn failed() -> impl Filter<Extract = (Response,), Error = Rejection> + Clone + 'static {
    warp::get().and_then(move || ready(Err(reject::not_found())))
}

/// Creates a [Filter][warp::Filter] that serves an included directory.
///
/// See the documentation of [static_dir] for details.
pub fn dir<'a, R: Read + Seek + Clone + Send + Sync>(
    dir: &'a ZipArchive<R>,
) -> impl Filter<Extract = (Response,), Error = Rejection> + Clone + 'a {
    warp::get()
        // use the unmatched tail to decide which path we're serving
        .and(tail().and_then(move |tail: Tail| ready(parse_path(tail.as_str(), dir))))
        // extract conditional information for cached / range responses
        .and(headers_cloned().map(|h: HeaderMap| Conditionals {
            if_modified_since: h.typed_get(),
            if_unmodified_since: h.typed_get(),
            if_range: h.typed_get(),
            range: h.typed_get(),
        }))
        // reply with the file's content, if it exists
        .and_then(move |path, conds| ready(reply(dir, path, conds)))
}

#[doc(hidden)]
pub fn file<'a, R: Read + Seek + Clone + Send + Sync>(
    dir: &'a ZipArchive<R>,
    file: &'static str,
) -> impl Filter<Extract = (Response,), Error = Rejection> + Clone + 'a {
    warp::get()
        // use the unmatched tail to decide which path we're serving
        .and_then(move || ready(parse_path(file, dir)))
        // extract conditional information for cached / range responses
        .and(headers_cloned().map(|h: HeaderMap| Conditionals {
            if_modified_since: h.typed_get(),
            if_unmodified_since: h.typed_get(),
            if_range: h.typed_get(),
            range: h.typed_get(),
        }))
        // reply with the file's content, if it exists
        .and_then(move |path, conds| ready(reply(dir, path, conds)))
}

fn parse_path<R: Read + Seek + Clone>(
    tail: &str,
    dir: &ZipArchive<R>,
) -> Result<String, Rejection> {
    let mut path_str = urlencoding::decode(tail).map_err(|_| reject::not_found())?;
    let path: PathBuf = path_str.split('/').collect();

    if path.parent().is_none()
        || dir
            .clone()
            .by_name(&path_str)
            .map(|v| v.is_dir())
            .unwrap_or(false)
    {
        log::debug!("appending index.html to {:?}", path);
        path_str += "/index.html";
    }

    Ok(path_str)
}

#[derive(Clone, Debug, Default)]
struct Conditionals {
    if_modified_since: Option<IfModifiedSince>,
    if_unmodified_since: Option<IfUnmodifiedSince>,
    if_range: Option<IfRange>,
    range: Option<Range>,
}

enum Cond {
    WithBody(Option<Range>),
    NoBody(Response),
}

impl Conditionals {
    fn check(self, sys_time: &SystemTime, last_mod: &LastModified) -> Cond {
        if let Some(since) = self.if_unmodified_since {
            if !since.precondition_passes(*sys_time) {
                let mut res = Response::new(Body::empty());
                *res.status_mut() = StatusCode::PRECONDITION_FAILED;
                return Cond::NoBody(res);
            }
        }

        if let Some(since) = self.if_modified_since {
            if !since.is_modified(*sys_time) {
                let mut res = Response::new(Body::empty());
                *res.status_mut() = StatusCode::NOT_MODIFIED;
                return Cond::NoBody(res);
            }
        }

        if let Some(if_range) = self.if_range {
            if if_range.is_modified(None, Some(last_mod)) {
                return Cond::WithBody(None);
            }
        }

        Cond::WithBody(self.range)
    }
}

fn bytes_range(range: Range, max_len: u64) -> Option<(u64, u64)> {
    use std::ops::Bound;

    let (start, end) = range.iter().next()?;

    let start = match start {
        Bound::Unbounded => 0,
        Bound::Included(s) => s,
        Bound::Excluded(s) => s + 1,
    };

    let end = match end {
        Bound::Unbounded => max_len,
        Bound::Included(s) => s + 1,
        Bound::Excluded(s) => s,
    };

    if start < end && end <= max_len {
        Some((start, end))
    } else {
        Some((0, max_len))
    }
}

fn reply<R: Read + Seek + Clone>(
    dir: &ZipArchive<R>,
    path: String,
    conds: Conditionals,
) -> Result<Response, Rejection> {
    let mut contents = Vec::new();
    if let Ok(mut file) = dir.clone().by_name(&path) {
        file.read_to_end(&mut contents).unwrap();
        log::debug!("serving: {:?}", path);
        Ok(reply_conditional(contents, &path, conds))
    } else {
        log::warn!("file not found: {:?}", path);
        Err(reject::not_found())
    }
}

fn reply_conditional(buf: Vec<u8>, path: &str, conds: Conditionals) -> Response {
    static SYS_TIME: Lazy<SystemTime> = Lazy::new(|| SystemTime::now());
    static LAST_MOD: Lazy<LastModified> = Lazy::new(|| (*SYS_TIME).into());

    let len = buf.len() as u64;

    match conds.check(&SYS_TIME, &LAST_MOD) {
        Cond::WithBody(Some(range)) => match bytes_range(range, buf.len() as u64) {
            Some((0, e)) if e == len => reply_full(buf, path, *LAST_MOD),
            Some(range) => reply_range(buf, range, path, *LAST_MOD),
            None => reply_unsatisfiable(len),
        },
        Cond::WithBody(None) => reply_full(buf, path, *LAST_MOD),
        Cond::NoBody(resp) => resp,
    }
}

fn reply_full(buf: Vec<u8>, path: &str, last_mod: LastModified) -> Response {
    let length = buf.len() as u64;
    let mut resp = Response::new(buf.into());
    let hdrs = resp.headers_mut();

    hdrs.typed_insert(ContentLength(length));
    let mime = mime_guess::from_path(path).first_or_octet_stream();
    hdrs.typed_insert(ContentType::from(mime));
    hdrs.typed_insert(AcceptRanges::bytes());
    hdrs.typed_insert(last_mod);

    resp
}

fn reply_range(buf: Vec<u8>, (s, e): (u64, u64), path: &str, last_mod: LastModified) -> Response {
    let len = e - s;
    let buf = (&buf[s as usize..e as usize]).to_owned();

    let mut resp = Response::new(Body::from(buf));
    *resp.status_mut() = StatusCode::PARTIAL_CONTENT;

    let hdrs = resp.headers_mut();
    hdrs.typed_insert(ContentRange::bytes(s..e, len).unwrap());
    hdrs.typed_insert(ContentLength(len));

    let mime = mime_guess::from_path(path).first_or_octet_stream();
    hdrs.typed_insert(ContentType::from(mime));
    hdrs.typed_insert(AcceptRanges::bytes());
    hdrs.typed_insert(last_mod);

    resp
}

fn reply_unsatisfiable(len: u64) -> Response {
    let mut resp = Response::new(Body::empty());

    *resp.status_mut() = StatusCode::RANGE_NOT_SATISFIABLE;
    resp.headers_mut()
        .typed_insert(ContentRange::unsatisfied_bytes(len));

    resp
}
