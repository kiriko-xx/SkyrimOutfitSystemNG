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

    // Tells the player, in a modal box at the point where it is known, that the illusion
    // is not in place this session, and why it is not: either a hook could not be
    // installed (a_atLoad, reported right after the hooks were applied) or the hooks are
    // in place but the game never took a single piece of the display set, which would
    // leave the player nude (reported from the health check in Patches.cpp). The details
    // stay in the log either way.
    void ShowIllusionUnavailableBox(bool a_atLoad);
}// namespace Hooking

#endif//SKYRIMOUTFITSYSTEMSE_SRC_HOOKING_HOOKS_HPP
