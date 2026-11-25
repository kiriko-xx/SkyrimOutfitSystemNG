use crate::{interface::ffi::*, stable_ptr::TESObjectARMOStablePtr};
use commonlibsse::{
    RE_LookupARMOFormID, RE_TESDataHandler_GetSingleton, RE_TESDataHandler_LookupFormIDC,
};
use configparser::ini::Ini;
use hash_hasher::HashedSet;
use log::*;
use once_cell::sync::Lazy;
use parking_lot::RwLock;
use std::{ffi::CString, path::PathBuf};

pub struct Settings {
    ini: Ini,
}

impl Settings {
    pub fn new() -> Self {
        let mut file = PathBuf::from(GetRuntimeDirectory().to_string());
        file.push("Data\\SKSE\\Plugins\\SkyrimOutfitSystemNG.ini");
        let mut ini = Ini::new();
        ini.set_multiline(true);
        if let Err(error) = ini.load(file.clone()) {
            warn!(
                "Could not load INI file at {} due to {}. Defaults will be used.",
                file.display(),
                error
            );
        } else {
            info!("Loaded INI file in {}", file.display());
        }
        Settings { ini }
    }

    pub fn extra_logging_enabled(&self) -> bool {
        self.ini
            .getboolcoerce("Debug", "ExtraLogging")
            .unwrap_or_else(|_| None)
            .unwrap_or_else(|| false)
    }

    #[allow(dead_code)]
    pub fn script_hot_reloading_enabled(&self) -> bool {
        self.ini
            .getboolcoerce("Debug", "ScriptHotReloading")
            .unwrap_or_else(|_| None)
            .unwrap_or_else(|| false)
    }

    pub fn default_base_policy(&self) -> Option<String> {
        self.ini.get("Customization", "NewOutfitDefaultBasePolicy")
    }

    pub fn should_set_default_per_slot_policy(&self) -> bool {
        self.ini
            .getboolcoerce("Customization", "NewOutfitSetDefaultSlotPolicies")
            .unwrap_or_else(|_| None)
            .unwrap_or_else(|| true)
    }

    pub fn enable_server(&self) -> bool {
        self.ini
            .getboolcoerce("Server", "Enable")
            .unwrap_or_else(|_| None)
            .unwrap_or_else(|| false)
    }

    pub fn server_port(&self) -> u16 {
        self.ini
            .get("Server", "Port")
            .and_then(|v| v.parse().ok())
            .unwrap_or_else(|| 3030)
    }

    pub fn server_remote_accessible(&self) -> bool {
        self.ini
            .getboolcoerce("Server", "AllowRemoteAccess")
            .unwrap_or_else(|_| None)
            .unwrap_or_else(|| false)
    }

    pub fn server_refresh_immediately(&self) -> bool {
        self.ini
            .getboolcoerce("Server", "RefreshImmediately")
            .unwrap_or_else(|_| None)
            .unwrap_or_else(|| false)
    }
}

pub static SETTINGS: Lazy<Settings> = Lazy::new(|| Settings::new());

pub struct IntragameCaches {
    pub golden_items: HashedSet<TESObjectARMOStablePtr>,
}

impl IntragameCaches {
    pub fn new() -> Self {
        let golden_items = Self::generate_golden_items(&SETTINGS.ini);
        IntragameCaches { golden_items }
    }

    fn generate_golden_items(ini: &Ini) -> HashedSet<TESObjectARMOStablePtr> {
        // Start with the set of built-in locators
        let mut locators = vec![
            ("Dragonborn.esm", 0x00021739), // Dragon Aspect Armor
        ];
        // Load locators from INI file.
        let ini_string = ini
            .get("Compatibility", "GoldenItems")
            .unwrap_or_else(|| "".to_string());

        locators.extend(ini_string.split(",").flat_map(|entry| {
            entry.split_once(":").and_then(|(name, form_id)| {
                u32::from_str_radix(form_id, 16)
                    .ok()
                    .map(|form_id| (name.trim(), form_id))
            })
        }));

        let data_handler = unsafe { RE_TESDataHandler_GetSingleton() };
        if data_handler.is_null() {
            return Default::default();
        }
        let items = locators
            .into_iter()
            .flat_map(|(mod_name, local_form_id)| {
                let Ok(mod_name) = CString::new(mod_name) else { return None };
                // TODO: Change this to using RE_TESDataHandler_LookupFormIDRawC
                let form_id = unsafe {
                    RE_TESDataHandler_LookupFormIDC(data_handler, local_form_id, mod_name.as_ptr())
                };
                if form_id != 0 {
                    let armor = unsafe { RE_LookupARMOFormID(form_id) };
                    unsafe { TESObjectARMOStablePtr::new(armor) }
                } else {
                    None
                }
            })
            .collect();
        items
    }
}

pub static INTRAGAME_CACHES: Lazy<RwLock<IntragameCaches>> =
    Lazy::new(|| RwLock::new(IntragameCaches::new()));
