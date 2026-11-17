use log::*;
use std::{
    io::{Cursor, Read},
    path::PathBuf,
};

use crate::{interface::ffi::GetRuntimeDirectory, settings::SETTINGS};
use warp::Filter;
use warp::Reply;
use warp_static_zip::ZipArchive;

pub async fn web_server() {
    let get_state = warp::path!("state")
        .and(warp::get())
        .and_then(handlers::get_state);
    let set_state = warp::path!("state")
        .and(warp::post())
        .and(warp::query::<handlers::SetStateParams>())
        .and(warp::body::bytes())
        .and_then(handlers::set_state);
    let get_outfit_default = warp::path!("outfits" / "default")
        .and(warp::get())
        .and_then(handlers::get_outfit_default);
    let armor_info = warp::path!("armors")
        .and(warp::query::<handlers::ArmorLocatorWarp>())
        .and_then(handlers::armor_info);
    let armor_search = warp::path!("armors" / "search")
        .and(warp::query::<handlers::ArmorSearch>())
        .and_then(handlers::armor_search);
    let actor_info = warp::path!("actors" / u32).and_then(handlers::actor_info);
    let policy_info = warp::path!("policies").and_then(handlers::policy_info);

    let base = warp::path("api");
    let routes = get_state
        .or(set_state)
        .or(get_outfit_default)
        .or(armor_info)
        .or(armor_search)
        .or(actor_info)
        .or(policy_info);

    let static_asset_routes = create_zip_route();

    let addr = if SETTINGS.server_remote_accessible() {
        [0, 0, 0, 0]
    } else {
        [127, 0, 0, 1]
    };
    let binding = warp::serve(base.and(routes).or(static_asset_routes))
        .try_bind_ephemeral((addr, SETTINGS.server_port()));
    match binding {
        Ok((addr, sock)) => {
            info!("Server bound to {}", addr);
            sock.await;
        }
        Err(err) => {
            warn!("Could not bind address: {}", err);
        }
    };
}

fn create_zip_route() -> warp::filters::BoxedFilter<(impl Reply,)> {
    let zip_vector = {
        let mut file = PathBuf::from(GetRuntimeDirectory().to_string());
        file.push("Data\\SKSE\\Plugins\\SkyrimOutfitSystemSE_Web.zip");
        match std::fs::File::open(file) {
            Ok(mut zip_data) => {
                let mut data = Vec::new();
                if let Err(error) = zip_data.read_to_end(&mut data) {
                    error!("Failed to read web archive file: {}", error);
                };
                data
            }
            Err(error) => {
                error!("Failed to read zip archive file: {}", error);
                Vec::new()
            }
        }
    };

    let zip_archive = Box::leak(Box::new(ZipArchive::new(Cursor::new(&*zip_vector.leak()))));
    let static_asset_routes = {
        match zip_archive {
            Ok(archive) => {
                info!("Successfully read web zip archive");
                warp_static_zip::dir(&*archive) // Handle most static paths
                    .or(warp_static_zip::file(&*archive, "index.html"))
                    .unify()
                    .boxed()
            }
            Err(error) => {
                error!("Failed to parse zip archive: {}", error);
                warp_static_zip::failed().boxed()
            }
        }
    };
    static_asset_routes
}

mod handlers {
    use crate::{
        armor::{ArmorExt, ArmorLocatorExt},
        interface::ffi::{ArmorSearch, CreateRefreshAllArmorsTask},
        outfit::Outfit,
        policy::POLICY_STORE,
        settings::SETTINGS,
        stable_ptr::TESObjectARMOStablePtr,
        OUTFIT_SERVICE_SINGLETON,
    };
    use commonlibsse::{
        RE_LookupActorFormID, RE_TESDataHandler_GetSingleton, SKSE_GetSerializationInterface,
    };
    use protobuf_json_mapping::PrintOptions;
    use protos::outfit::ArmorLocator;
    use serde::{Deserialize, Serialize};
    use std::{convert::Infallible, ffi::CStr};
    use warp::hyper::StatusCode;

    pub async fn get_state() -> Result<impl warp::Reply, Infallible> {
        let system = OUTFIT_SERVICE_SINGLETON.read().save();
        let Ok(json) = protobuf_json_mapping::print_to_string_with_options(&system, &{
            let mut opts = PrintOptions::default();
            opts.always_output_default_values = true;
            opts
        }) else {
            return Ok(warp::reply::with_status("Unable to serialize".to_owned(), StatusCode::INTERNAL_SERVER_ERROR));
        };
        Ok(warp::reply::with_status(json, StatusCode::OK))
    }

    #[derive(Deserialize)]
    pub struct SetStateParams {
        refresh_armors: Option<bool>,
    }

    pub async fn set_state(
        params: SetStateParams,
        body: bytes::Bytes,
    ) -> Result<impl warp::Reply, Infallible> {
        let Ok(body) = String::from_utf8(body.to_vec()) else {
            return Ok(warp::reply::with_status("Failed", StatusCode::BAD_REQUEST));
        };
        let intfc = unsafe { SKSE_GetSerializationInterface() };
        let result = OUTFIT_SERVICE_SINGLETON
            .write()
            .replace_with_json(body.as_str(), unsafe { &*intfc });
        // Try to set the refresh all armors task... unsure if this is a good idea.
        if params.refresh_armors.unwrap_or(false) || SETTINGS.server_refresh_immediately() {
            CreateRefreshAllArmorsTask();
        }
        if result {
            Ok(warp::reply::with_status("OK", StatusCode::OK))
        } else {
            Ok(warp::reply::with_status("Failed", StatusCode::BAD_REQUEST))
        }
    }

    pub async fn get_outfit_default() -> Result<impl warp::Reply, Infallible> {
        let outfit = Outfit::new("");
        let outfit = outfit.save();
        let Ok(json) = protobuf_json_mapping::print_to_string_with_options(&outfit, &{
            let mut opts = PrintOptions::default();
            opts.always_output_default_values = true;
            opts
        }) else {
            return Ok(warp::reply::with_status("Unable to serialize".to_owned(), StatusCode::INTERNAL_SERVER_ERROR))
        };
        Ok(warp::reply::with_status(json, StatusCode::OK))
    }

    // Armor routes
    #[derive(Serialize, Deserialize)]
    #[serde(rename_all = "camelCase")]
    pub struct ArmorLocatorWarp {
        pub raw_form_id: u32,
        pub local_form_id: u32,
        pub mod_name: String,
    }

    #[derive(Serialize)]
    struct ArmorInfo {
        pub locator: ArmorLocatorWarp,
        pub name: String,
        pub mask: u32,
    }

    pub async fn armor_info(locator_in: ArmorLocatorWarp) -> Result<impl warp::Reply, Infallible> {
        let locator = {
            let mut new_locator = ArmorLocator::new();
            new_locator.raw_form_id = locator_in.raw_form_id;
            new_locator.local_form_id = locator_in.local_form_id;
            new_locator.mod_name = locator_in.mod_name.clone();
            new_locator
        };
        let data_handler = unsafe { RE_TESDataHandler_GetSingleton() };
        assert!(
            !data_handler.is_null(),
            "Could not get TESDataHandler for loading!"
        );
        let Some(armor_ptr) = locator.get_armor(unsafe { &mut *data_handler }) else {
            return Ok(warp::reply::with_status(warp::reply::json(&"Cannot find armor"), StatusCode::NOT_FOUND));
        };
        let name = unsafe {
            let cstr = CStr::from_ptr(armor_ptr.borrow()._base._base._base.GetName());
            cstr.to_str()
                .map(|s| s.to_owned())
                .unwrap_or_else(|_| "".to_owned())
        };
        let mask = unsafe { armor_ptr.borrow()._base_10.GetSlotMask() } as u32;
        let info = ArmorInfo {
            locator: locator_in,
            name,
            mask,
        };
        Ok(warp::reply::with_status(
            warp::reply::json(&info),
            StatusCode::OK,
        ))
    }

    #[derive(Deserialize)]
    pub struct ArmorSearch {
        pub search: String,
    }

    pub async fn armor_search(search: ArmorSearch) -> Result<impl warp::Reply, Infallible> {
        let data_handler = unsafe { RE_TESDataHandler_GetSingleton() };
        assert!(
            !data_handler.is_null(),
            "Could not get TESDataHandler for loading!"
        );
        let armors = ArmorSearch(&search.search);
        let armors: Vec<_> = armors
            .into_iter()
            .filter_map(|armor| unsafe { TESObjectARMOStablePtr::new(armor.ptr) })
            .filter_map(|armor| {
                Some(ArmorInfo {
                    locator: unsafe { armor.borrow() }.get_armor_locator().map(|loc| {
                        ArmorLocatorWarp {
                            raw_form_id: loc.raw_form_id,
                            local_form_id: loc.local_form_id,
                            mod_name: loc.mod_name,
                        }
                    })?,
                    name: unsafe {
                        let cstr = CStr::from_ptr(armor.borrow()._base._base._base.GetName());
                        cstr.to_str()
                            .map(|s| s.to_owned())
                            .unwrap_or_else(|_| "".to_owned())
                    },
                    mask: unsafe { armor.borrow()._base_10.GetSlotMask() } as u32,
                })
            })
            .collect();
        Ok(warp::reply::with_status(
            warp::reply::json(&armors),
            StatusCode::OK,
        ))
    }

    #[derive(Serialize)]
    struct ActorInfo {
        pub name: String,
    }

    pub async fn actor_info(actor_form_id: u32) -> Result<impl warp::Reply, Infallible> {
        let actor = unsafe { RE_LookupActorFormID(actor_form_id) };
        if actor.is_null() {
            return Ok(warp::reply::with_status(
                warp::reply::json(&"Cannot find actor"),
                StatusCode::NOT_FOUND,
            ));
        };
        let name = unsafe {
            let cstr = CStr::from_ptr((*actor)._base.GetName());
            cstr.to_str()
                .map(|s| s.to_owned())
                .unwrap_or_else(|_| "".to_owned())
        };
        let info = ActorInfo { name };
        Ok(warp::reply::with_status(
            warp::reply::json(&info),
            StatusCode::OK,
        ))
    }

    #[derive(Serialize)]
    struct PolicyInfo {
        value: String,
        ui_name: String,
        sort_order: u8,
        advanced: bool,
    }

    // Get Policy Info
    pub async fn policy_info() -> Result<impl warp::Reply, Infallible> {
        let policies: Vec<_> = POLICY_STORE
            .list_available_policies(true)
            .into_iter()
            .map(|p| PolicyInfo {
                value: p.value.0.to_string(),
                ui_name: p.ui_name.to_string(),
                sort_order: p.sort_order,
                advanced: p.advanced,
            })
            .collect();
        Ok(warp::reply::with_status(
            warp::reply::json(&policies),
            StatusCode::OK,
        ))
    }
}
