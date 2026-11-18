//
// Created by m on 9/11/2022.
//

#ifndef SKYRIMOUTFITSYSTEMSE_SRC_HOOKING_HOOKS_HPP
#define SKYRIMOUTFITSYSTEMSE_SRC_HOOKING_HOOKS_HPP

namespace SKSE {
    class Trampoline;
}

namespace Hooking {
    extern SKSE::Trampoline* g_localTrampoline;
    extern SKSE::Trampoline* g_branchTrampoline;

    // Installs the five skinning hooks. Single implementation for every supported
    // runtime: the game addresses come from REL::RelocationID(seID, aeID), so the
    // SE and AE builds share one code path and one address table.
    void ApplyPlayerSkinningHooks();

    // Installs the per-frame anchor for the morph refresh. The engine calls
    // Main::Update once per frame; that call site is redirected through a trampoline
    // which runs the morph poll before the update, so the poll runs on the game
    // thread no matter what the SKSE task queue does. Returns false when no call was
    // found or the trampoline refused the write; the morph refresh then keeps using
    // its timer.
    bool ApplyFramePumpHook();
}// namespace Hooking

#endif//SKYRIMOUTFITSYSTEMSE_SRC_HOOKING_HOOKS_HPP
