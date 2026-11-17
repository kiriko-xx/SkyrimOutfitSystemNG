use crate::stable_ptr::{REStableFormPtr, TESObjectARMOStablePtr};
use commonlibsse::{
    RE_LookupARMOFormID, RE_TESDataHandler, RE_TESDataHandler_LookupFormIDC, RE_TESObjectARMO,
};
use log::*;
use protos::outfit::ArmorLocator;
use std::ffi::{CStr, CString};

pub trait ArmorLocatorExt {
    fn get_armor(&self, data_handler: &mut RE_TESDataHandler) -> Option<TESObjectARMOStablePtr>;
}

impl ArmorLocatorExt for ArmorLocator {
    fn get_armor(&self, data_handler: &mut RE_TESDataHandler) -> Option<TESObjectARMOStablePtr> {
        let Ok(mod_name) = CString::new(self.mod_name.as_str()) else { return None };
        // TODO: Change this to using RE_TESDataHandler_LookupFormIDRawC
        let form_id = unsafe {
            RE_TESDataHandler_LookupFormIDC(data_handler, self.local_form_id, mod_name.as_ptr())
        };
        if form_id != 0 {
            let armor = unsafe { RE_LookupARMOFormID(form_id) };
            if let Some(armor) = unsafe { TESObjectARMOStablePtr::new(armor) } {
                Some(armor)
            } else {
                warn!(
                    "Saved FormID {} resulted in no armor. Skipping.",
                    self.local_form_id
                );
                None
            }
        } else {
            warn!(
                "Saved FormID {} could not be matched. Skipping.",
                self.local_form_id
            );
            None
        }
    }
}

pub trait ArmorExt {
    fn get_armor_locator(&self) -> Option<ArmorLocator>;
}

impl ArmorExt for RE_TESObjectARMO {
    fn get_armor_locator(&self) -> Option<ArmorLocator> {
        // TODO: Change this to using GetRawFormID exclusively
        let local_form_id = unsafe { self._base._base._base.GetLocalFormID() };
        let raw_form_id = unsafe { self._base._base._base.GetRawFormID() };
        let file = unsafe { REStableFormPtr::new(self._base._base._base.GetFile(0)) };
        let Some(file) = file else {
            warn!(
                "Raw FormID {} for an armor has no filename. Skipping.",
                raw_form_id
            );
            return None;
        };
        let filename = unsafe { CStr::from_ptr(&file.borrow().fileName as *const i8) };
        let mod_name = filename.to_string_lossy().into_owned();
        Some(ArmorLocator {
            raw_form_id,
            local_form_id,
            mod_name,
            ..ArmorLocator::new()
        })
    }
}
