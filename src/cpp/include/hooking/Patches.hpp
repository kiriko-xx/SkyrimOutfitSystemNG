//
// Created by m on 9/11/2022.
//

#ifndef SKYRIMOUTFITSYSTEMSE_INCLUDE_HOOKING_PATCHES_HPP
#define SKYRIMOUTFITSYSTEMSE_INCLUDE_HOOKING_PATCHES_HPP

#include "RE/REAugments.h"

namespace Hooking {
    // The illusion is only applied while both player skinning hooks are in place:
    // DontVanillaSkinPlayer hides the real armour, so without CustomSkinPlayer nothing
    // is attached in its place and the actor renders nude. The installer reports the
    // hook state here once it has run; until then the override stays off.
    void SetIllusionDisplayEnabled(bool a_enabled);
    bool IllusionDisplayEnabled();

    bool ShouldOverrideSkinning(RE::TESObjectREFR* target);

    namespace DontVanillaSkinPlayer {
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

        // Uses the inventory owner and the model supplied by the engine call site.
        void CustomFromInventoryModel(RE::InventoryChanges* inventory, RE::ActorWeightModel* actorWeightModel);

        // actorWeightModel may be null, in which case both of the actor's weight models
        // (third person, then first person) are attached.
        void Custom(RE::Actor* target, RE::ActorWeightModel* actorWeightModel);

        // Whether the installer found a call site inside the game's skinning routine.
        // When it did not, the display set is attached from the deferred queue below:
        // the worn mask query and the skinning pass still run for every actor, so the
        // illusion can be applied right after the pass that hid the real armour.
        void SetSiteHookInstalled(bool a_installed);
        bool SiteHookInstalled();

        // Asks for one attach pass for this actor at the end of the frame. Cheap to call
        // repeatedly: an actor that is already queued is ignored.
        void MarkAttachPending(RE::Actor* a_actor);
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
