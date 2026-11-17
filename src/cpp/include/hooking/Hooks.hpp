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
}// namespace Hooking

#endif//SKYRIMOUTFITSYSTEMSE_SRC_HOOKING_HOOKS_HPP
