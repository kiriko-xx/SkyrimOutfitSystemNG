use std::cmp::Eq;
use std::fmt::Debug;
use std::hash::Hash;

use commonlibsse::RE_TESObjectARMO;

pub type TESObjectARMOStablePtr = REStableFormPtr<RE_TESObjectARMO>;

/// A wrapper for a pointer to a TESForm class in Skyrim that is *stable in memory*, which can be used to get shared references to the class.
/// Creation is unsafe, requiring that the pointee has static lifetime and is not mutated by the game while any reference is live. The
/// `new` method will check that the pointer is not null.
pub struct REStableFormPtr<T> {
    ptr: *const T,
}

impl<T> REStableFormPtr<T> {
    pub unsafe fn new(ptr: *const T) -> Option<Self> {
        if ptr.is_null() {
            return None;
        };
        Some(Self { ptr })
    }

    pub unsafe fn borrow(&self) -> &T {
        unsafe { &*self.ptr }
    }

    pub fn as_ptr(&self) -> *const T {
        self.ptr
    }
}

impl<T> Hash for REStableFormPtr<T> {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.ptr.hash(state);
    }
}

impl<T> PartialEq for REStableFormPtr<T> {
    fn eq(&self, other: &Self) -> bool {
        self.ptr == other.ptr
    }
}

impl<T> Clone for REStableFormPtr<T> {
    fn clone(&self) -> Self {
        Self {
            ptr: self.ptr.clone(),
        }
    }
}

impl<T> Copy for REStableFormPtr<T> {}

impl<T> Debug for REStableFormPtr<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("REStableFormPtr")
            .field("ptr", &self.ptr)
            .finish()
    }
}

impl<T> Eq for REStableFormPtr<T> {}

unsafe impl<T> Send for REStableFormPtr<T> {}
unsafe impl<T> Sync for REStableFormPtr<T> {}
