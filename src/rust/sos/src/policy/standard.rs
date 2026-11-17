use crate::policy::{EvaluatorConstructorVTable, Metadata, PolicyEvaluator};
use commonlibsse::{RE_BIPED_OBJECTS_BIPED_OBJECT_kEditorTotal, RE_TESObjectARMO};
use std::any::Any;

pub static EVALUATOR_VTABLE: EvaluatorConstructorVTable = EvaluatorConstructorVTable {
    evaluator_resources_creation_func: &Evaluator::create_resources,
    evaluator_instance_creation_func: &Evaluator::new_boxed,
};

pub struct Evaluator {}

impl Evaluator {
    pub fn create_resources() -> Box<dyn Any + Send> {
        Box::new(())
    }
    pub fn new_boxed(_: &mut (dyn Any + Send)) -> Box<dyn PolicyEvaluator> {
        Box::new(Evaluator {})
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
        let policy_str = current_metadata.value.name().as_str();
        let has_equipped = !equipped[current_index as usize].is_null();
        let has_outfit = !outfit[current_index as usize].is_null();
        let code = match (has_equipped, has_outfit) {
            (false, false) => policy_str.chars().nth(0),
            (true, false) => policy_str.chars().nth(1),
            (false, true) => policy_str.chars().nth(2),
            (true, true) => policy_str.chars().nth(3),
        };
        match code {
            Some('E') => Some(equipped[current_index as usize]),
            Some('O') => Some(outfit[current_index as usize]),
            _ => None,
        }
    }
}
