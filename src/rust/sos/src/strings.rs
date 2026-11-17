use cxx::CxxString;
use natord;
use std::cmp::Ordering;
use uncased::Uncased;
pub type UncasedString = Uncased<'static>;

pub fn nat_ord_case_insensitive_c(a: &CxxString, b: &CxxString) -> i8 {
    let a = a.to_string_lossy();
    let b = b.to_string_lossy();
    match natord::compare_ignore_case(&a, &b) {
        Ordering::Less => 1,
        Ordering::Equal => 0,
        Ordering::Greater => -1,
    }
}
