//
// Created by m on 9/11/2022.
//

#ifndef SKYRIMOUTFITSYSTEMSE_INCLUDE_HOOKING_PATCHES_HPP
#define SKYRIMOUTFITSYSTEMSE_INCLUDE_HOOKING_PATCHES_HPP

#include <cstdint>

#include "RE/REAugments.h"

namespace Hooking {
    // What the illusion is allowed to do this session. Its two halves sit on different
    // functions and fail for different reasons, so they are reported and degraded
    // separately instead of one switch that turns everything off:
    //   off        - nothing is hidden, nothing is attached, the real armour shows;
    //   attachOnly - the display set is attached inside the skinning pass, but the hook
    //                that hides the real armour is missing, so the illusion is drawn on top
    //                of the real armour instead of replacing it;
    //   full       - both halves are in place.
    // There is deliberately no "hide only": hiding the real armour with nothing to put in
    // its place shows a nude actor, so an attach half that fails while the hide half is
    // installed still means off.
    enum class IllusionMode : std::uint8_t { off = 0, attachOnly = 1, full = 2 };

    void SetIllusionMode(IllusionMode a_mode);
    IllusionMode IllusionState();

    // Convenience for the callers that only know "on" and "off" (the health check switching
    // the illusion off, and anything that reads the switch as a boolean).
    void SetIllusionDisplayEnabled(bool a_enabled);
    bool IllusionDisplayEnabled();

    bool ShouldOverrideSkinning(RE::TESObjectREFR* target);

    namespace DontVanillaSkinPlayer {
        // The register-level gate, called from inside the hooked armour pass with the
        // pieces the game itself passed (armour, weight model). It resolves the actor from
        // the weight model, attaches the display set at the start of the pass and answers
        // whether this piece is to be hidden.
        bool ShouldHideForModel(RE::TESObjectARMO* armor, RE::ActorWeightModel* model);

        // The same question in terms of an actor, used by the deferred path.
        bool ShouldOverride(RE::TESObjectARMO* armor, RE::TESObjectREFR* target);
    }

    namespace ShimWornFlags {
        // Called from inside the game's worn-mask query with the inventory it is being
        // asked about, so the owner is taken from the inventory instead of a register.
        // Returns the vanilla mask when the actor has no illusion to report.
        std::uint32_t OverrideWornFlags(RE::InventoryChanges* inventory);
    }

    namespace CustomSkinPlayer {
        // Entry point used by the hook: the visitation runs for one inventory, and the
        // actor plus both of its weight models are resolved from it.
        void CustomFromInventory(RE::InventoryChanges* inventory);

        // Entry point of the worn-visitor hook: the injected stub runs the game's own pass
        // first and then calls this with the same inventory and visitor, so the biped holds
        // the real armour and the routine that called the pass finishes the parts after
        // this returns. This is the only place the display set is written.
        void CustomFromWornPass(RE::InventoryChanges* inventory, RE::InventoryChanges::IItemChangeVisitor* visitor);

        // The vtable of the visitor the pass that fills the biped is handed, so a visitor
        // belonging to another pass is recognised instead of read.
        void SetWornVisitorVTable(std::uintptr_t a_vtable);

        // actorWeightModel may be null, in which case both of the actor's weight models
        // (third person, then first person) are attached.
        void Custom(RE::Actor* target, RE::ActorWeightModel* actorWeightModel);

        // Whether the installer managed to hook the worn pass. The deferred queue below
        // never writes a biped itself - it only asks the game to rebuild one, so that the
        // pass (and this hook) runs again.
        void SetSiteHookInstalled(bool a_installed);
        bool SiteHookInstalled();

        // Asks for one attach pass for this actor at the end of the frame. Cheap to call
        // repeatedly: an actor that is already queued is ignored.
        void MarkAttachPending(RE::Actor* a_actor);

        // Slots of the display set this plugin last applied whose clone is no longer in
        // the third person biped: the illusion was thrown away without a skinning pass
        // this plugin could attach from. Main thread only; 0 when nothing is missing.
        std::uint32_t IllusionMissingSlots(RE::Actor* a_actor);
    }

    namespace FixEquipConflictCheck {
        void Inner(std::uint32_t bodyPartForNewItem, RE::Actor* target);
        bool ShouldOverride(RE::TESForm* item);
    }// namespace FixEquipConflictCheck

    namespace FixSkillLeveling {
        struct Visitor;
        bool Inner(RE::BipedAnim* biped, Visitor* bipedVisitor);
    }// namespace FixSkillLeveling

    namespace RTTIPrinter {
        void Print_RTTI(RE::InventoryChanges::IItemChangeVisitor* target);
    }
}// namespace Hooking

#endif//SKYRIMOUTFITSYSTEMSE_INCLUDE_HOOKING_PATCHES_HPP
