use self::scripting::SCRIPT_STORE;
use crate::interface::ffi::MetadataC;
use commonlibsse::{RE_BIPED_OBJECTS_BIPED_OBJECT_kEditorTotal, RE_TESObjectARMO};
use once_cell::sync::Lazy;
use smartstring::{LazyCompact, SmartString};
use std::{any::Any, collections::HashMap};

#[derive(Clone, Ord, PartialOrd, Eq, PartialEq, Hash)]
pub struct Policy(pub SmartString<LazyCompact>);

impl Policy {
    pub fn from_name(name: &str) -> Self {
        Policy(SmartString::from(name))
    }

    pub fn from_int(value: u8) -> Option<Self> {
        let string_value = match value {
            0 => SmartString::from("XXXX"),
            1 => SmartString::from("XXXE"),
            2 => SmartString::from("XXXO"),
            3 => SmartString::from("XXOX"),
            4 => SmartString::from("XXOE"),
            5 => SmartString::from("XXOO"),
            6 => SmartString::from("XEXX"),
            7 => SmartString::from("XEXE"),
            8 => SmartString::from("XEXO"),
            9 => SmartString::from("XEOX"),
            10 => SmartString::from("XEOE"),
            11 => SmartString::from("XEOO"),
            n if n >= 128 => SmartString::from(format!("CustomPolicy{:03}", n - 127)),
            _n => return None,
        };
        Some(Policy(string_value))
    }

    pub fn is_built_in_policy_name(&self) -> bool {
        match self.name().as_str() {
            "XXXX" => true,
            "XXXE" => true,
            "XXXO" => true,
            "XXOX" => true,
            "XXOE" => true,
            "XXOO" => true,
            "XEXX" => true,
            "XEXE" => true,
            "XEXO" => true,
            "XEOX" => true,
            "XEOE" => true,
            "XEOO" => true,
            _ => false,
        }
    }

    pub fn name(&self) -> &SmartString<LazyCompact> {
        &self.0
    }
}

#[derive(Clone)]
pub struct Metadata {
    /// Index that is serialized to select this policy. Should be unique.
    pub value: Policy,
    pub ui_name: SmartString<LazyCompact>,
    pub sort_order: u8,
    pub advanced: bool,
    /// A vtable pointing to the constructor table for the evaluators associated to this type. Can be shared between multiple policies,
    /// all policies with the same `evaluator_instance_key` should use the same evaluator vtable.
    pub evaluator_ctor_table: &'static EvaluatorConstructorVTable,
}

#[derive(Clone)]
pub struct EvaluatorConstructorVTable {
    /// A function that creates long-lived resources used by evaluators.
    pub evaluator_resources_creation_func: &'static (dyn Fn() -> Box<dyn Any + Send> + Send + Sync),
    /// A function that creates a new evaluator if the service doesn't have one already.
    pub evaluator_instance_creation_func:
        &'static (dyn Fn(&mut (dyn Any + Send)) -> Box<dyn PolicyEvaluator> + Send + Sync),
}

pub trait PolicyEvaluator {
    fn select(
        &mut self,
        equipped: &[*const RE_TESObjectARMO; RE_BIPED_OBJECTS_BIPED_OBJECT_kEditorTotal as usize],
        outfit: &[*const RE_TESObjectARMO; RE_BIPED_OBJECTS_BIPED_OBJECT_kEditorTotal as usize],
        current_metadata: &Metadata,
        current_index: u32,
    ) -> Option<*const RE_TESObjectARMO>;
}

impl From<Metadata> for MetadataC {
    fn from(meta: Metadata) -> Self {
        MetadataC {
            name: meta.value.name().to_string(),
            sort_order: meta.sort_order,
            advanced: meta.advanced,
        }
    }
}

pub struct PolicyStore {
    metadata: HashMap<SmartString<LazyCompact>, Metadata>,
}

impl PolicyStore {
    pub fn new() -> PolicyStore {
        let mut metadata = vec![
            Metadata {
                value: Policy::from_name("XXXX"),
                ui_name: SmartString::from("XXXX"),
                sort_order: 100,
                advanced: true,
                evaluator_ctor_table: &standard::EVALUATOR_VTABLE,
            }, // Never show anything
            Metadata {
                value: Policy::from_name("XXXE"),
                ui_name: SmartString::from("XXXE"),
                sort_order: 101,
                advanced: true,
                evaluator_ctor_table: &standard::EVALUATOR_VTABLE,
            }, // If outfit and equipped, show equipped
            Metadata {
                value: Policy::from_name("XXXO"),
                ui_name: SmartString::from("XXXO"),
                sort_order: 2,
                advanced: false,
                evaluator_ctor_table: &standard::EVALUATOR_VTABLE,
            }, // If outfit and equipped, show outfit (require equipped, no passthrough)
            Metadata {
                value: Policy::from_name("XXOX"),
                ui_name: SmartString::from("XXOX"),
                sort_order: 102,
                advanced: true,
                evaluator_ctor_table: &standard::EVALUATOR_VTABLE,
            }, // If only outfit, show outfit
            Metadata {
                value: Policy::from_name("XXOE"),
                ui_name: SmartString::from("XXOE"),
                sort_order: 103,
                advanced: true,
                evaluator_ctor_table: &standard::EVALUATOR_VTABLE,
            }, // If only outfit, show outfit. If both, show equipped
            Metadata {
                value: Policy::from_name("XXOO"),
                ui_name: SmartString::from("XXOO"),
                sort_order: 1,
                advanced: false,
                evaluator_ctor_table: &standard::EVALUATOR_VTABLE,
            }, // If outfit, show outfit (always show outfit, no passthough)
            Metadata {
                value: Policy::from_name("XEXX"),
                ui_name: SmartString::from("XEXX"),
                sort_order: 104,
                advanced: true,
                evaluator_ctor_table: &standard::EVALUATOR_VTABLE,
            }, // If only equipped, show equipped
            Metadata {
                value: Policy::from_name("XEXE"),
                ui_name: SmartString::from("XEXE"),
                sort_order: 105,
                advanced: true,
                evaluator_ctor_table: &standard::EVALUATOR_VTABLE,
            }, // If equipped, show equipped
            Metadata {
                value: Policy::from_name("XEXO"),
                ui_name: SmartString::from("XEXO"),
                sort_order: 3,
                advanced: false,
                evaluator_ctor_table: &standard::EVALUATOR_VTABLE,
            }, // If only equipped, show equipped. If both, show outfit
            Metadata {
                value: Policy::from_name("XEOX"),
                ui_name: SmartString::from("XEOX"),
                sort_order: 106,
                advanced: true,
                evaluator_ctor_table: &standard::EVALUATOR_VTABLE,
            }, // If only equipped, show equipped. If only outfit, show outfit
            Metadata {
                value: Policy::from_name("XEOE"),
                ui_name: SmartString::from("XEOE"),
                sort_order: 107,
                advanced: true,
                evaluator_ctor_table: &standard::EVALUATOR_VTABLE,
            }, // If only equipped, show equipped. If only outfit, show outfit. If both, show equipped
            Metadata {
                value: Policy::from_name("XEOO"),
                ui_name: SmartString::from("XEOO"),
                sort_order: 108,
                advanced: true,
                evaluator_ctor_table: &standard::EVALUATOR_VTABLE,
            }, // If only equipped, show equipped. If only outfit, show outfit. If both, show outfit
        ];
        let mut custom_items_sorted: Vec<_> = SCRIPT_STORE.scripts.iter().map(|(_, v)| v).collect();
        custom_items_sorted.sort_by_key(|v| &v.ui_name);
        let mut custom_items: Vec<_> = custom_items_sorted
            .into_iter()
            .enumerate()
            .map(|(index, record)| {
                let policy = Policy::from_name(&record.name);
                Metadata {
                    value: policy,
                    ui_name: SmartString::from(&record.ui_name),
                    sort_order: index as u8 + 128,
                    advanced: false,
                    evaluator_ctor_table: &scripting::EVALUATOR_VTABLE,
                }
            })
            .collect();
        for custom_item in &custom_items {
            log::info!(
                "Added custom script policy. ID={}, Sort={}",
                custom_item.value.0,
                custom_item.sort_order,
            );
        }
        for new_item in &custom_items {
            if let Some(overwritten_item) = metadata.iter().find(|m| m.value == new_item.value) {
                log::warn!("A built-in policy '{}' is being overridden with a script implementation. This will work, but this is only unofficially supported and may be unexpected.", overwritten_item.value.name())
            }
        }
        metadata.append(&mut custom_items);
        let metadata_name_lut = build_metadata_name_map(&metadata);
        PolicyStore {
            metadata: metadata_name_lut,
        }
    }

    pub fn list_available_policies(&self, allow_advanced: bool) -> Vec<&Metadata> {
        let mut filtered: Vec<_> = self
            .metadata
            .values()
            .filter(|p| allow_advanced || !p.advanced)
            .collect();
        filtered.sort_by_key(|v| v.sort_order);
        filtered
    }
}

// TODO: We want to minimize direct accesses of this as much as we can.
pub static POLICY_STORE: Lazy<PolicyStore> = Lazy::new(|| PolicyStore::new());

fn build_metadata_name_map(metadata: &[Metadata]) -> HashMap<SmartString<LazyCompact>, Metadata> {
    metadata
        .iter()
        .map(|m| (m.value.0.clone(), m.clone()))
        .collect()
}

pub fn list_available_policies_c(allow_advanced: bool) -> Vec<MetadataC> {
    POLICY_STORE
        .list_available_policies(allow_advanced)
        .into_iter()
        .cloned()
        .map(|m| m.into())
        .collect()
}

pub fn translation_key_c(policy: &str) -> String {
    Policy::from_name(policy).translation_key(&*POLICY_STORE)
}

impl Policy {
    pub fn policy_with_ui_key<'a, 'b>(store: &'a PolicyStore, code: &'b str) -> Option<&'a Self> {
        store.metadata.get(code).map(|v| &v.value)
    }

    pub fn policy_metadata<'a, 'b>(&'b self, store: &'a PolicyStore) -> Option<&'a Metadata> {
        store.metadata.get(&self.0)
    }

    pub fn translation_key(&self, store: &PolicyStore) -> String {
        let Some(metadata) = self.policy_metadata(store) else {
            return "$SkyOutSys_Desc_EasyPolicyName_UNKNOWN".to_owned()
        };
        if self.is_built_in_policy_name() {
            "$SkyOutSys_Desc_EasyPolicyName_".to_owned() + metadata.ui_name.as_str()
        } else {
            // If the author made their name a translation key, then we respect that.
            let ui_name = &metadata.ui_name;
            if ui_name.chars().nth(0).unwrap_or('0') == '$' {
                ui_name.to_string()
            } else {
                "*".to_owned() + ui_name.as_str()
            }
        }
    }
}

mod scripting;
mod standard;
