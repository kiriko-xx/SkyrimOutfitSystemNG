use crate::interface::ffi::GetRuntimeDirectory;
use crate::policy::{EvaluatorConstructorVTable, Metadata, PolicyEvaluator};
use crate::settings::SETTINGS;
use crate::stable_ptr::TESObjectARMOStablePtr;
use commonlibsse::{
    RE_BIPED_OBJECTS_BIPED_OBJECT_kEditorTotal, RE_BSFixedString_CStr, RE_TESObjectARMO,
};
use hash_hasher::HashedMap;
use once_cell::sync::Lazy;
use regex::Regex;
use rhai::plugin::*;
use rhai::{CallFnOptions, Engine, Module, Scope, AST};
use smartstring::{LazyCompact, SmartString};
use std::any::Any;
use std::borrow::Cow;
use std::collections::HashMap;
use std::error::Error;
use std::path::PathBuf;

pub struct ScriptStore {
    engine: Engine,
    pub scripts: HashedMap<SmartString<LazyCompact>, ScriptRecord>,
}

#[derive(Clone)]
pub struct ScriptRecord {
    pub name: SmartString<LazyCompact>,
    pub ui_name: String,
    pub path: PathBuf,
    pub ast: Option<AST>,
}

impl ScriptStore {
    pub fn new_global() -> Self {
        let result = Self::new();
        match result {
            Ok(store) => store,
            Err(error) => {
                log::error!("Failed to create script store: {}", error);
                Self {
                    engine: Engine::new(),
                    scripts: Default::default(),
                }
            }
        }
    }

    pub fn new() -> Result<Self, Box<dyn Error>> {
        log::info!("Collecting scripts");
        let engine = Engine::new();
        let mut scripts: HashedMap<SmartString<LazyCompact>, ScriptRecord> = Default::default();
        let mut file = PathBuf::from(GetRuntimeDirectory().to_string());
        file.push("Data\\SKSE\\Plugins\\");
        let filename_regex = Regex::new(r"SkyrimOutfitSystem_PolicyScript_(\w+).rhai")
            .expect("Failed to compile regex");
        for file in std::fs::read_dir(file)? {
            if scripts.len() >= 127 {
                log::warn!("You have reached or exceeded the maximum number of scripts (the limit is 127).");
                break;
            }
            let Ok(file) = file else { continue };
            let filename = file.file_name();
            let path = file.path();
            let Some(str) = filename.to_str() else { continue };
            let Some(captures) = filename_regex.captures(str) else { continue };
            let name = &captures[1];
            let (ast, ui_name) = match engine.compile_file(file.path()) {
                Ok(ast) => {
                    let mut scope = Scope::new();
                    if let Err(error) = engine.run_file_with_scope(&mut scope, file.path()) {
                        log::error!(
                            "Failed to get display name for script {:?} due to: {}",
                            file.path(),
                            error
                        );
                    };
                    let ui_name = match scope.get_value("name") {
                        Some(ui_name) => ui_name,
                        None => {
                            log::info!("Script {} did not provide a display name property.", str);
                            str.to_owned()
                        }
                    };
                    (Some(ast), ui_name)
                }
                Err(error) => {
                    log::error!(
                        "Failed to compile AST for script {:?} due to: {}",
                        file.path(),
                        error
                    );
                    if SETTINGS.script_hot_reloading_enabled() {
                        log::info!(
                            "Since we are in hot-reload mode, we will make the script available anyway."
                        );
                        (None, str.to_owned())
                    } else {
                        continue;
                    }
                }
            };
            if let Some((_, conflict_script)) =
                scripts.iter().find(|(_, other)| other.ui_name == ui_name)
            {
                log::error!(
                    "Script {} has the same name as {}. This is not allowed. {} will be ignored.",
                    path.to_string_lossy(),
                    conflict_script.path.to_string_lossy(),
                    path.to_string_lossy(),
                );
                continue;
            }
            scripts.insert(
                SmartString::from(name),
                ScriptRecord {
                    name: SmartString::from(name),
                    path,
                    ui_name: ui_name,
                    ast: ast,
                },
            );
        }
        log::info!("Loaded {} scripts.", scripts.len());
        Ok(Self { engine, scripts })
    }

    pub fn get_ast(&self, name: &str) -> Option<(&ScriptRecord, Cow<AST>)> {
        let Some(script) = self.scripts.get(name) else {
            log::error!("Got invalid script name = {}", name);
            return None;
        };
        if SETTINGS.script_hot_reloading_enabled() {
            let compile_result = self.engine.compile_file(script.path.clone());
            match compile_result {
                Ok(ast) => Some((script, Cow::Owned(ast))),
                Err(error) => {
                    log::error!(
                        "Failed to compile script Name={} Path={:?}: {}",
                        script.name,
                        script.path,
                        error
                    );
                    None
                }
            }
        } else {
            if let Some(ast) = &script.ast {
                Some((script, Cow::Borrowed(ast)))
            } else {
                log::error!(
                    "No compiled AST found for script Name={} Path={:?}",
                    script.name,
                    script.path
                );
                return None;
            }
        }
    }
}

// TODO: We want to minimize direct accesses of this as much as we can.
pub static SCRIPT_STORE: Lazy<ScriptStore> = Lazy::new(|| ScriptStore::new_global());

pub static EVALUATOR_VTABLE: EvaluatorConstructorVTable = EvaluatorConstructorVTable {
    evaluator_resources_creation_func: &Evaluator::create_resources,
    evaluator_instance_creation_func: &Evaluator::new_boxed,
};

// TODO: Evaluators only exist for a single run of "compute_display_set". "Resources" are meant to cache longer term resources.
// We eventually want to cache the engine and ASTs using that. Note that we *don't* want to cache the scopes. Those should exist
// only for the evalutator's lifetime.
pub struct Evaluator {
    engine: Engine,
    precalculated: HashMap<
        SmartString<LazyCompact>,
        [Option<*const RE_TESObjectARMO>; RE_BIPED_OBJECTS_BIPED_OBJECT_kEditorTotal as usize],
    >,
}

impl Evaluator {
    pub fn create_resources() -> Box<dyn Any + Send> {
        Box::new(())
    }
    pub fn new_boxed(_: &mut (dyn Any + Send)) -> Box<dyn PolicyEvaluator> {
        let mut engine = Engine::new();

        engine.on_print(|text| log::info!("Script Log: {text}"));

        // Register the plugin module declared below
        let module = exported_module!(scripting_types);
        engine.register_global_module(module.into());

        Box::new(Evaluator {
            engine,
            precalculated: HashMap::new(),
        })
    }
}

impl PolicyEvaluator for Evaluator {
    fn select(
        &mut self,
        equipped: &[*const RE_TESObjectARMO; RE_BIPED_OBJECTS_BIPED_OBJECT_kEditorTotal as usize],
        outfit: &[*const RE_TESObjectARMO; RE_BIPED_OBJECTS_BIPED_OBJECT_kEditorTotal as usize],
        current_metadata: &Metadata,
        current_index: u32,
    ) -> Option<*const RE_TESObjectARMO> {
        let Some((record, ast)) = SCRIPT_STORE.get_ast(&current_metadata.value.name()) else {
            log::error!("No compiled AST found for script name = {}", current_metadata.value.name());
            return None;
        };
        let mut scope = Scope::new();
        let precalculated = self.precalculated.entry(current_metadata.value.name().clone()).or_insert_with(|| {
            let equipped_dyn: Vec<_> = equipped
                .map(|ptr| {
                    if let Some(ptr) = unsafe { TESObjectARMOStablePtr::new(ptr) } {
                        Some(scripting_types::Armor { ptr })
                    } else {
                        None
                    }
                })
                .into_iter()
                .map(|v| {
                    if let Some(v) = v {
                        Dynamic::from(v)
                    } else {
                        Dynamic::from(())
                    }
                })
                .collect();

            let outfit_dyn: Vec<_> = outfit
                .map(|ptr| {
                    if let Some(ptr) = unsafe { TESObjectARMOStablePtr::new(ptr) } {
                        Some(scripting_types::Armor { ptr })
                    } else {
                        None
                    }
                })
                .into_iter()
                .map(|v| {
                    if let Some(v) = v {
                        Dynamic::from(v)
                    } else {
                        Dynamic::from(())
                    }
                })
                .collect();

            let selected = match self.engine.call_fn_with_options::<Vec<Dynamic>>(
                CallFnOptions::new(),
                &mut scope,
                &ast,
                "select_armors",
                (
                    scripting_types::Context {},
                    equipped_dyn,
                    outfit_dyn,
                ),
            ) {
                Ok(value) => value,
                Err(error) => {
                    log::error!(
                    "Script {} failed to select slot: {}",
                    record.ui_name,
                    error
                );
                    return [None; RE_BIPED_OBJECTS_BIPED_OBJECT_kEditorTotal as usize];
                }
            };

            let vector: Vec<_> = selected.into_iter().map(|selected| {
                if selected.is_unit() {
                    None
                } else if let Some(outfit) = selected.try_cast::<scripting_types::Armor>() {
                    Some(outfit.ptr.as_ptr())
                } else {
                    log::error!(
                        "Script {} did not return unit type or armor for slot {}.",
                        record.ui_name,
                        current_index
                    );
                    None
                }
            }).collect();

            match vector.try_into() {
                Ok(array) => array,
                Err(vec) => {
                    log::error!(
                        "Script {} did not return the correct number of items. Expected {}, got {}.",
                        vec.len(),
                        record.ui_name,
                        RE_BIPED_OBJECTS_BIPED_OBJECT_kEditorTotal as usize
                    );
                    [None; RE_BIPED_OBJECTS_BIPED_OBJECT_kEditorTotal as usize]
                }
            }
        });
        precalculated[current_index as usize]
    }
}

#[export_module]
mod scripting_types {
    use std::ffi::CStr;

    use crate::stable_ptr::TESObjectARMOStablePtr;

    #[derive(Clone, Debug)]
    pub struct ScriptingArmor {
        pub ptr: TESObjectARMOStablePtr,
    }
    pub type Armor = ScriptingArmor;

    pub fn name(value: &mut Armor) -> String {
        unsafe {
            let cstr = CStr::from_ptr(value.ptr.borrow()._base._base._base.GetName());
            cstr.to_str()
                .map(|s| s.to_owned())
                .unwrap_or_else(|_| "".to_owned())
        }
    }

    pub fn local_form_id(value: &mut Armor) -> u32 {
        unsafe { value.ptr.borrow()._base._base._base.GetLocalFormID() }
    }

    pub fn mod_name(value: &mut Armor) -> String {
        let file = unsafe { value.ptr.borrow()._base._base._base.GetFile(0) };
        if file.is_null() {
            "".to_string()
        } else {
            unsafe {
                CStr::from_ptr(&(*file).fileName as *const i8)
                    .to_string_lossy()
                    .to_string()
            }
        }
    }

    pub fn keywords(value: &mut Armor) -> rhai::Array {
        unsafe {
            let keyword_count = value.ptr.borrow()._base_12.GetNumKeywords();
            (0..keyword_count)
                .map(|i| *(value.ptr.borrow()._base_12.keywords.add(i as usize)))
                .map(|keyword_ptr| RE_BSFixedString_CStr(&(*keyword_ptr).formEditorID))
                .filter(|ptr| !ptr.is_null())
                .map(|nonnull_ptr| CStr::from_ptr(nonnull_ptr).to_string_lossy())
                .map(|str| rhai::ImmutableString::from(str.as_ref()))
                .map(|string| Dynamic::from(string))
                .collect()
        }
    }

    #[derive(Clone, Debug)]
    pub struct ScriptingContext {}
    pub type Context = ScriptingContext;
}
