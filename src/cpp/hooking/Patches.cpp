//
// Created by m on 9/11/2022.
//

#include "hooking/Patches.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "GameState.h"
#include "MorphRefresh.h"
#include "Utility.h"
#include "bindings.h"
#include "core/Settings.h"

#include <SKSE/SKSE.h>

namespace Hooking {
    SKSE::Trampoline* g_localTrampoline = nullptr;
    SKSE::Trampoline* g_branchTrampoline = nullptr;

    namespace {
        // Off until the hook installer reports that both player skinning hooks are in
        // place. A failed install must never leave the real armour hidden behind an
        // illusion that nobody applies: that shows up as a nude actor.
        std::atomic_bool g_illusionDisplayEnabled{ false };

        // The skinning trampoline hands us the actor register as-is, so a hook that
        // ends up on a relocated call site must not dereference a pointer that is not
        // backed by readable memory. Cheap enough to run once per skinning pass.
        bool IsReadableObject(const void* a_pointer) {
            const auto address = reinterpret_cast<std::uintptr_t>(a_pointer);
            if (address < 0x10000 || (address & 0x7) != 0) return false;
            MEMORY_BASIC_INFORMATION info{};
            if (::VirtualQuery(a_pointer, &info, sizeof(info)) == 0) return false;
            if (info.State != MEM_COMMIT) return false;
            if ((info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) return false;
            const auto regionEnd = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
            return address + 0x20 <= regionEnd;
        }
    }

    void SetIllusionDisplayEnabled(bool a_enabled) {
        g_illusionDisplayEnabled.store(a_enabled, std::memory_order_relaxed);
    }

    bool IllusionDisplayEnabled() {
        return g_illusionDisplayEnabled.load(std::memory_order_relaxed);
    }

    bool ShouldOverrideSkinning(RE::TESObjectREFR* target) {
        // Keeps the vanilla skinning pass untouched unless the illusion can actually be
        // applied; see SetIllusionDisplayEnabled.
        if (!IllusionDisplayEnabled()) {
            return false;
        }
        // Never touch actors while a save is being loaded: the biped data is
        // half constructed there and a refresh at that point hangs the game.
        if (SOS::GameState::IsLoading()) {
            return false;
        }
        LogExit exitPrint("ShouldOverrideSkinning"sv);
        auto service = outfit_service_get_singleton_ptr();
        if (!target) {
            LOG(warn, "Target was null");
            return false;
        }
        if (!IsReadableObject(target)) {
            LOG(warn, "ShouldOverrideSkinning: 0x{:X} is not a readable object; ignoring the call.",
                reinterpret_cast<std::uintptr_t>(target));
            return false;
        }
        if (!service->inner().enabled_c()) return false;
            auto actor = skyrim_cast<RE::Actor*>(target);
        if (!actor) {
            LOG(warn, "Target failed to cast to RE::Actor");
            return false;
        }
        if (!service->inner().should_override(actor->GetFormID())) return false;
        return true;
    }

    class EquippedArmorVisitor: public RE::IItemChangeVisitorAugment {
        //
        // If the player has a shield equipped, and if we're not overriding that
        // shield, then we need to grab the equipped shield's worn-flags.
        //
    public:
        EquippedArmorVisitor()
        {
            equipped.reserve(32);
        }

        virtual VisitorReturn Visit(RE::InventoryEntryData* data) override {
            auto form = data ? data->object : nullptr;
            if (form && form->formType == RE::FormType::Armor) {
                equipped.push_back(skyrim_cast<RE::TESObjectARMO*>(form));
            }
            return VisitorReturn::kContinue;// Return true to "continue visiting".
        };

        std::vector<const RE::TESObjectARMO*> equipped;
    };

    template<typename F>
    class EquippedVisitorFn: public RE::IItemChangeVisitorAugment {
        //
        // If the player has a shield equipped, and if we're not overriding that
        // shield, then we need to grab the equipped shield's worn-flags.
        //
    public:
        explicit EquippedVisitorFn(F callable) : callable(callable){};
        virtual VisitorReturn Visit(RE::InventoryEntryData* data) override {
            auto form = data ? data->object : nullptr;
            if (form) {
                callable(data->object);
            }
            return VisitorReturn::kContinue;// Return true to "continue visiting".
        };

        F callable;
    };

    namespace DontVanillaSkinPlayer {
        bool ShouldOverride(RE::TESObjectARMO* armor, RE::TESObjectREFR* target) {
            LogExit exitPrint("DontVanillaSkinPlayer.ShouldOverride"sv);
            if (!ShouldOverrideSkinning(target)) { return false; }
            auto svc = outfit_service_get_singleton_ptr();
            auto actor = skyrim_cast<RE::Actor*>(target);
            if (!actor) {
                // Actor failed to cast...
                LOG(warn, "ShouldOverride: Failed to cast target to Actor.");
                return true;
            }
            auto outfit = svc->inner().current_outfit_ptr(actor->GetFormID());
            if (!outfit) return false;
            auto inventory = target->GetInventoryChanges();
            EquippedArmorVisitor visitor;
            if (inventory) {
                RE::InventoryChangesAugments::ExecuteAugmentVisitorOnWorn(inventory, &visitor);
            } else {
                LOG(warn, "ShouldOverride: Unable to get target inventory.");
                return true;
            }
            // Block the item (return true) if the item isn't in the display set.
                        auto displaySet = outfit->compute_display_set_c(rust::Slice{visitor.equipped.data(), visitor.equipped.size()});
            for (const auto displayItem: displaySet) {
                if (displayItem.ptr == armor) return false;
            }
            // This piece is being hidden, so the illusion has to take its place.
            CustomSkinPlayer::MarkAttachPending(actor);
            return true;
        }
    }// namespace DontVanillaSkinPlayer

    namespace ShimWornFlags {
        // The hook replaces the game's own call to InventoryChanges::GetWornMask, so a
        // mask has to come back on every path: the displayed slots while this actor is
        // being drawn with an illusion, and the untouched vanilla mask otherwise. The
        // owner comes from the inventory the game asked about, which is why this no
        // longer needs the actor to sit in a particular register.
        std::uint32_t OverrideWornFlags(RE::InventoryChanges* a_inventory) {
            LogExit exitPrint("ShimWornFlags.OverrideWornFlags"sv);
            if (!a_inventory) return 0;
            // Also covers the cases where the illusion is switched off for this session
            // or the actor has no outfit: those must see the real mask, not zero.
            if (!ShouldOverrideSkinning(a_inventory->owner)) return a_inventory->GetWornMask();
            auto* actor = skyrim_cast<RE::Actor*>(a_inventory->owner);
            if (!actor) return a_inventory->GetWornMask();
            auto svc = outfit_service_get_singleton_ptr();
            auto outfit = svc->inner().current_outfit_ptr(actor->GetFormID());
            if (!outfit) return a_inventory->GetWornMask();
            EquippedArmorVisitor visitor;
            RE::InventoryChangesAugments::ExecuteAugmentVisitorOnWorn(a_inventory, &visitor);
            auto displaySet = outfit->compute_display_set_c(rust::Slice{visitor.equipped.data(), visitor.equipped.size()});
            std::uint32_t mask = 0;
            for (const auto& item : displaySet) {
                if (item.ptr) mask |= item.ptr->GetSlotMask().underlying();
            }
            if (mask != 0) {
                // The engine just asked what this actor wears while it was drawing it:
                // on a build without a call site to hook, this is the moment the attach
                // pass gets scheduled for.
                CustomSkinPlayer::MarkAttachPending(actor);
            }
            return mask;
        }
    }// namespace ShimWornFlags

    namespace CustomSkinPlayer {
        // ApplyArmorAddon stores the *outfit* armour in BipedAnim::objects[slot].item.
        // Mods that look at the biped to decide what can be damaged (Procedural Armor
        // Damage collects the worn armours from the inventory and only keeps slots
        // whose item is one of them) therefore never see the illusion. The mesh comes
        // from partClone/addon, so writing the real armour back into item keeps the
        // appearance while the bookkeeping stays honest.
        void RestoreRealItems(RE::Actor* a_actor, const RE::TESObjectARMO* a_applied, const std::array<const RE::TESForm*, 32>& a_wornBySlot) {
            if (!a_actor || !a_applied) return;
            const auto mask = a_applied->GetSlotMask().underlying();
            if (mask == 0) return;

            for (const bool firstPerson : { false, true }) {
                auto& biped = a_actor->GetBiped(firstPerson);
                auto* anim = biped.get();
                if (!anim) continue;
                for (std::uint32_t bit = 0; bit < 32; ++bit) {
                    if ((mask & (1u << bit)) == 0) continue;
                    auto& slot = anim->objects[bit];
                    // Only undo our own write: if the same armour is genuinely worn in
                    // the slot the item already points at it and must stay untouched.
                    if (slot.item == a_applied && a_wornBySlot[bit]) {
                        slot.item = const_cast<RE::TESForm*>(a_wornBySlot[bit]);
                    }
                }
            }
        }

        // Detaches every slot in the mask. ClearBipedPart removes the node and
        // partClone; nulling item/addon/part forces ApplyArmorAddon to build a
        // brand new clone instead of treating the slot as already up to date
        // (which would keep the old .tri shape).
        //
        // CustomOne() runs once per ActorWeightModel (third person first, then
        // first person). The second argument is the address of the actor's
        // BSTSmartPointer<BipedAnim> holder, not a BipedAnim itself. Clear only
        // that model: clearing both models here wiped the model rebuilt by the
        // previous call, which made the body and clothing vanish until the outfit
        // was applied again.
        void ClearSlots(RE::ActorWeightModel* a_model, std::uint32_t a_mask) {
            if (!a_model || a_mask == 0) return;
            auto* holder = reinterpret_cast<RE::BSTSmartPointer<RE::BipedAnim>*>(a_model);
            auto* anim = holder ? holder->get() : nullptr;
            if (!anim) return;
            for (std::uint32_t bit = 0; bit < 32; ++bit) {
                if ((a_mask & (1u << bit)) == 0) continue;
                auto& slot = anim->objects[bit];
                RE::BipedAnimAugments::ClearBipedPart(anim, &slot, true, 0);
                slot.item = nullptr;
                slot.addon = nullptr;
                slot.part = nullptr;
            }
        }

        // Returning the clone node of a slot makes a rebuild visible in the log:
        // a new pointer after ClearSlots plus ApplyArmorAddon proves the slot got
        // a fresh geometry instead of the stale .tri shape it held before.
        const void* SlotClone(RE::ActorWeightModel* a_model, std::uint32_t a_bit) {
            auto* holder = reinterpret_cast<RE::BSTSmartPointer<RE::BipedAnim>*>(a_model);
            auto* anim = holder ? holder->get() : nullptr;
            if (!anim || a_bit >= 32) return nullptr;
            return static_cast<const void*>(anim->objects[a_bit].partClone.get());
        }

        // The store revision changes whenever an outfit is created, edited or
        // reassigned - including the "wear this outfit again" path the player
        // triggers from the quick picker or a hotkey. Such a re-apply has to
        // rebuild every displayed piece, otherwise the illusion keeps the
        // geometry that was cloned for the previous body shape. The window is
        // per actor and lasts a couple of seconds because Custom runs once per
        // ActorWeightModel (third person, then first person).
        std::mutex g_revisionMutex;
        std::unordered_map<RE::FormID, std::uint64_t> g_seenRevision;
        std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> g_revisionHot;

        bool RevisionRebuildPending(RE::FormID a_actor, std::uint64_t a_revision) {
            const auto now = std::chrono::steady_clock::now();
            std::lock_guard lock(g_revisionMutex);
            if (g_seenRevision.size() > 512) g_seenRevision.clear();
            const auto [it, inserted] = g_seenRevision.try_emplace(a_actor, a_revision);
            if (!inserted && it->second != a_revision) {
                it->second = a_revision;
                g_revisionHot[a_actor] = now + std::chrono::milliseconds(2000);
            }
            const auto hot = g_revisionHot.find(a_actor);
            if (hot == g_revisionHot.end()) return false;
            if (hot->second <= now) {
                g_revisionHot.erase(hot);
                return false;
            }
            return true;
        }

        // Slots the outfit occupied the last time Custom ran for this weight model.
        // Slots that drop out of the display set (policy changed to X, piece removed,
        // outfit switched) keep their cloned mesh and item until something detaches
        // them, and DontVanillaSkinPlayer keeps the real armour out of those slots.
        std::mutex g_appliedMaskMutex;
        std::unordered_map<std::uintptr_t, std::uint32_t> g_appliedMasks;

        std::uint32_t TakeStaleSlots(RE::ActorWeightModel* a_model, std::uint32_t a_displayMask) {
            std::lock_guard lock(g_appliedMaskMutex);
            if (g_appliedMasks.size() > 256) g_appliedMasks.clear();
            const auto key = reinterpret_cast<std::uintptr_t>(a_model);
            const auto [it, inserted] = g_appliedMasks.try_emplace(key, a_displayMask);
            const std::uint32_t stale = inserted ? 0 : (it->second & ~a_displayMask);
            it->second = a_displayMask;
            return stale;
        }

        // Attaches the illusion to one weight model. Custom() below resolves the models
        // for an actor and calls this once per model, in the order the game does it
        // (third person, then first person).
        void CustomOne(RE::Actor* actor, RE::ActorWeightModel* actorWeightModel) {
            LogExit exitPrint("CustomSkinPlayer.CustomOne"sv);
            // Get basic actor information (race and sex)
            if (!actor || !actorWeightModel)
                return;
            // The actor's current race and sex, not the NPC record's: SetRace, vampire
            // lord and werewolf forms would otherwise pick the wrong armour addons.
            auto* race = actor->GetRace();
            if (!race)
                return;
            auto* actorBase = actor->GetActorBase();
            if (!actorBase)
                return;
            const bool isFemale = actorBase->GetSex() == RE::SEX::kFemale;
            //
            auto svc = outfit_service_get_singleton_ptr();
            auto outfit = svc->inner().current_outfit_ptr(actor->GetFormID());

            // Get actor inventory and equipped items
            auto inventory = actor->GetInventoryChanges();
            EquippedArmorVisitor visitor;
            if (inventory) {
                RE::InventoryChangesAugments::ExecuteAugmentVisitorOnWorn(inventory, &visitor);
            } else {
                LOG(debug, "Custom: Unable to get the actor inventory.");
                return;
            }

            // Map slot -> real armour before anything is applied; used below to put
            // the honest item back after the addon was swapped in, and to restore the
            // real piece when a slot leaves the display set.
            std::array<const RE::TESForm*, 32> wornBySlot{};
            for (const auto armor : visitor.equipped) {
                if (!armor) continue;
                const auto mask = armor->GetSlotMask().underlying();
                for (std::uint32_t bit = 0; bit < 32; ++bit) {
                    if ((mask & (1u << bit)) != 0) wornBySlot[bit] = armor;
                }
            }
            const bool restoreItems = SOS::Settings::Get().compatProceduralArmorDamage;

            std::uint32_t displayMask = 0;
            bool fullRebuild = false;
            std::vector<const RE::TESObjectARMO*> applySet;
            std::vector<const RE::TESObjectARMO*> rebuildSet;
            if (outfit) {
                // Compute the display set.
                auto displaySet = outfit->compute_display_set_c(rust::Slice{visitor.equipped.data(), visitor.equipped.size()});

                // Compute the remaining items to be applied to the player
                // We assume that the DontVanillaSkinPlayer already passed through
                // the equipped items that will be shown, so we only need to worry about the
                // items that are not equipped but which are in the outfit or which are masked
                // by the outfit.
                applySet.reserve(displaySet.size());
                for (const auto& item : displaySet) {
                    if (std::find(visitor.equipped.begin(), visitor.equipped.end(), item.ptr) == visitor.equipped.end()) applySet.push_back(item.ptr);
                }
                for (const auto& item : displaySet) {
                    if (item.ptr) displayMask |= item.ptr->GetSlotMask().underlying();
                }

                const bool revisionRebuild = RevisionRebuildPending(actor->GetFormID(), svc->inner().revision());
                const bool morphRebuild = SOS::MorphRefresh::ShouldForceRebuild(actor->GetFormID());
                fullRebuild = revisionRebuild || morphRebuild;

                // A body morph change must rebuild every displayed piece, not only
                // the outfit-only ones: an item that is genuinely equipped is still
                // part of displaySet and its cloned mesh is just as stale. The
                // non-forced path keeps the cheaper applySet behaviour.
                if (fullRebuild) {
                    rebuildSet.reserve(displaySet.size());
                    for (const auto& item : displaySet) {
                        if (item.ptr) rebuildSet.push_back(item.ptr);
                    }
                    LOG(debug, "Custom: rebuilding {} display pieces (mask 0x{:08X}, outfit revision {}, body morph {}) for {:08X}.", rebuildSet.size(), displayMask, revisionRebuild, morphRebuild, actor->GetFormID());
                } else {
                    rebuildSet = applySet;
                }
            }

            const std::uint32_t staleMask = TakeStaleSlots(actorWeightModel, displayMask);
            if (staleMask != 0) {
                ClearSlots(actorWeightModel, staleMask);
                std::array<const RE::TESObjectARMO*, 32> restored{};
                std::size_t restoredCount = 0;
                for (std::uint32_t bit = 0; bit < 32; ++bit) {
                    if ((staleMask & (1u << bit)) == 0 || !wornBySlot[bit]) continue;
                    auto* wornArmor = const_cast<RE::TESObjectARMO*>(wornBySlot[bit]->As<RE::TESObjectARMO>());
                    if (!wornArmor) continue;
                    if (std::find(restored.begin(), restored.begin() + restoredCount, wornArmor) != restored.begin() + restoredCount) continue;
                    restored[restoredCount++] = wornArmor;
                    RE::TESObjectARMOAugments::ApplyArmorAddon(wornArmor, race, actorWeightModel, isFemale);
                }
                LOG(debug, "Custom: {} slot(s) left the display set (mask 0x{:08X}); restored {} real piece(s) for {:08X}.", std::popcount(staleMask), staleMask, restoredCount, actor->GetFormID());
            }

            if (fullRebuild) {
                ClearSlots(actorWeightModel, displayMask);
            }

            for (const auto* armor : rebuildSet) {
                if (!armor) continue;
                RE::TESObjectARMOAugments::ApplyArmorAddon(armor, race, actorWeightModel, isFemale);
                if (restoreItems) {
                    RestoreRealItems(actor, armor, wornBySlot);
                }
                if (fullRebuild) {
                    const auto mask = armor->GetSlotMask().underlying();
                    for (std::uint32_t bit = 0; bit < 32; ++bit) {
                        if ((mask & (1u << bit)) == 0) continue;
                        LOG(debug, "Custom: {:08X} slot {:02} clone {}", armor->GetFormID(), bit, SlotClone(actorWeightModel, bit));
                    }
                }
            }
            // Every clone above was created straight through ApplyArmorAddon, so it
            // never went past skee's attach hook and carries no .tri morph target:
            // RaceMenu's ApplyMorphs() walks such a subtree without touching it.
            // Queues one re-morph for the slots just rebuilt (a main-thread task,
            // so nothing is re-entered from inside this hook).
            SOS::MorphRefresh::NotifySlotApplied(actor);
            if (fullRebuild) {
                SOS::MorphRefresh::NotifyRebuilt(actor);
            }
        }

        void Custom(RE::Actor* target, RE::ActorWeightModel* actorWeightModel) {
            if (!actorWeightModel) {
                LogExit exitPrint("CustomSkinPlayer.Custom"sv);
                auto* actor = skyrim_cast<RE::Actor*>(target);
                if (!actor) {
                    LOG(debug, "Custom: Failed to cast target to Actor.");
                    return;
                }
                // No weight model was handed in: attach both of the actor's biped models.
                // GetBiped() returns the actor's own BSTSmartPointer<BipedAnim> holder,
                // which is the pointer the game passes to this function, so the two paths
                // behave identically.
                for (const bool firstPerson : { false, true }) {
                    auto* holder = const_cast<RE::BSTSmartPointer<RE::BipedAnim>*>(&actor->GetBiped(firstPerson));
                    if (!holder || !holder->get()) continue;
                    CustomOne(actor, reinterpret_cast<RE::ActorWeightModel*>(holder));
                }
                return;
            }
            CustomOne(skyrim_cast<RE::Actor*>(target), actorWeightModel);
        }

        // Entry point used by the hook. It runs once per visitation of an actor's worn
        // items, and everything else - actor, race, weight models - is resolved from the
        // inventory, so the trampoline never depends on which register the game kept the
        // actor in.
        void CustomFromInventory(RE::InventoryChanges* a_inventory) {
            if (!a_inventory) return;
            auto* owner = a_inventory->owner;
            if (!ShouldOverrideSkinning(owner)) return;
            auto* actor = skyrim_cast<RE::Actor*>(owner);
            if (!actor) return;
            Custom(actor, nullptr);
        }

        namespace {
            std::atomic_bool g_siteHookInstalled{ false };

            // Actors whose attach pass is owed at the end of the frame, and whether a
            // task is already waiting to run them. Both are only touched on the main
            // thread, but the hooks can be reached from the render thread, so the
            // mutex stays.
            std::mutex g_attachMutex;
            std::unordered_set<RE::FormID> g_attachPending;
            // The last deferred attach of each actor. The engine asks for the worn mask
            // many times per skinning pass, so without this the whole display set would
            // be re-applied several times a frame for no visible gain.
            std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> g_attachLast;
            std::atomic_bool g_attachTaskQueued{ false };

            void DrainAttachPending() {
                g_attachTaskQueued.store(false, std::memory_order_relaxed);
                std::vector<RE::FormID> ids;
                {
                    std::lock_guard lock(g_attachMutex);
                    ids.assign(g_attachPending.begin(), g_attachPending.end());
                    g_attachPending.clear();
                }
                for (const auto id : ids) {
                    auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
                    if (!actor) continue;
                    if (!ShouldOverrideSkinning(actor)) continue;
                    Custom(actor, nullptr);
                }
            }
        }

        void SetSiteHookInstalled(bool a_installed) {
            g_siteHookInstalled.store(a_installed, std::memory_order_relaxed);
        }

        bool SiteHookInstalled() {
            return g_siteHookInstalled.load(std::memory_order_relaxed);
        }

        void MarkAttachPending(RE::Actor* a_actor) {
            if (!a_actor || SiteHookInstalled()) return;
            const auto id = a_actor->GetFormID();
            const auto now = std::chrono::steady_clock::now();
            bool firstOfBatch = false;
            {
                std::lock_guard lock(g_attachMutex);
                if (g_attachPending.size() > 512) g_attachPending.clear();
                if (g_attachLast.size() > 512) g_attachLast.clear();
                const auto last = g_attachLast.find(id);
                if (last != g_attachLast.end() && now < last->second) return;
                g_attachLast[id] = now + std::chrono::milliseconds(100);
                firstOfBatch = g_attachPending.empty();
                g_attachPending.insert(id);
            }
            if (firstOfBatch) {
                LOG(debug, "Custom: the skinning routine of this game build has no call site to hook; attaching the display set from the deferred pass.");
            }
            if (g_attachTaskQueued.exchange(true, std::memory_order_relaxed)) return;
            auto* tasks = SKSE::GetTaskInterface();
            if (!tasks) {
                g_attachTaskQueued.store(false, std::memory_order_relaxed);
                return;
            }
            tasks->AddTask([]() { DrainAttachPending(); });
        }
    }// namespace CustomSkinPlayer

    namespace FixEquipConflictCheck {
        //
        // When you try to equip an item, the game loops over the armors in your ActorWeightModel
        // rather than your other worn items. Because we're tampering with what goes into the AWM,
        // this means that conflict checks are run against your outfit instead of your equipment,
        // unless we patch in a fix. (For example, if your outfit doesn't include a helmet, then
        // you'd be able to stack helmets endlessly without this patch.)
        //
        // The loop in question is performed in Actor::Unk_120, which is also generally responsible
        // for equipping items at all.
        //
        class _Visitor: public RE::IItemChangeVisitorAugment {
            //
            // Bethesda used a visitor to add armor-addons to the ActorWeightModel in the first
            // place (see call stack for DontVanillaSkinPlayer patch), so why not use a similar
            // visitor to check for conflicts?
            //
        public:
            virtual VisitorReturn Visit(RE::InventoryEntryData* data) override {
                // TODO: [SlotPassthru] We might be able to leave this as-is.
                auto form = data ? data->object : nullptr;
                if (form && form->formType == RE::FormType::Armor) {
                    auto armor = skyrim_cast<RE::TESObjectARMO*>(form);
                    if (armor && RE::TESObjectARMOAugments::TestBodyPartByIndex(armor, this->conflictIndex)) {
                        auto em = RE::ActorEquipManager::GetSingleton();
                        if (!em) return VisitorReturn::kContinue;
                        //
                        // TODO: The third argument to this call is meant to be a
                        // BaseExtraList*, and Bethesda supplies one when calling from Unk_120.
                        // Can we get away with a nullptr here, or do we have to find the
                        // BaseExtraList that contains an ExtraWorn?
                        //
                        // I'm not sure how to investigate this, but I did run one test, and
                        // that works properly: I gave myself ten Falmer Helmets and applied
                        // different enchantments to two of them (leaving the others
                        // unenchanted). In tests, I was unable to stack the helmets with each
                        // other or with other helmets, suggesting that the BaseExtraList may
                        // not be strictly necessary.
                        //
                        em->UnequipObject(this->target, form, nullptr, 1, nullptr, false, false, true, false, nullptr);
                    }
                }
                return VisitorReturn::kContinue;// True to continue visiting
            };

            RE::Actor* target;
            std::uint32_t conflictIndex = 0;
        };
        void Inner(std::uint32_t bodyPartForNewItem, RE::Actor* target) {
            LogExit exitPrint("FixEquipConflictCheck.Inner"sv);
            if (!target) return;
            auto inventory = target->GetInventoryChanges();
            if (inventory) {
                _Visitor visitor;
                visitor.conflictIndex = bodyPartForNewItem;
                visitor.target = target;
                RE::InventoryChangesAugments::ExecuteAugmentVisitorOnWorn(inventory, &visitor);
            } else {
                LOG(debug, "OverridePlayerSkinning: Conflict check failed: no inventory!");
            }
        }
        bool ShouldOverride(RE::TESForm* item) {
            LogExit exitPrint("FixEquipConflictCheck.ShouldOverride"sv);
            //
            // We only hijack equipping for armors, so I'd like for this patch to only
            // apply to armors as well. It shouldn't really matter -- before I added
            // this check, weapons and the like tested in-game with no issues, likely
            // because they're handled very differently -- but I just wanna be sure.
            // We should use vanilla code whenever we don't need to NOT use it.
            //
            return item && item->formType == RE::FormType::Armor;
        }
    }// namespace FixEquipConflictCheck

    namespace FixSkillLeveling {
        struct Visitor {
            RE::TESObjectARMO** shield;
            RE::TESObjectARMO** torso;
            std::uint32_t light;
            std::uint32_t heavy;
        };
        static_assert(sizeof(Visitor) == 0x18);

        bool Inner(RE::BipedAnim* biped, Visitor* bipedVisitor) {
            LogExit exitPrint("FixSkillLeveling.Inner"sv);
            auto target = biped->actorRef.get();  // NiPointer<TESObjectREFR>, keeps the reference alive.
            if (!target) return false;
            auto actor = skyrim_cast<RE::Actor*>(target.get());
            if (!actor) return false;
            if (!ShouldOverrideSkinning(actor)) return false;
            auto inventory = target->GetInventoryChanges();
            if (!inventory) return false;
            EquippedVisitorFn inventoryVisitor([&](RE::TESBoundObject* form) {
                if (form->formType == RE::FormType::Armor) {
                    auto armor = skyrim_cast<RE::TESObjectARMO*>(form);
                    if (!armor) return;
                    RE::BIPED_MODEL::ArmorType armorType = armor->GetArmorType();
                    auto mask = armor->GetSlotMask().underlying();
                    // Exclude shields.
                    if (armor->IsShield()) return;
                    // Count the number of slots taken by the armor. Body slot counts double.
                    int count = 0;
                    if (mask & static_cast<std::uint32_t>(RE::BIPED_MODEL::BipedObjectSlot::kBody)) count++;
                    count += std::popcount(mask);
                    if (armorType == RE::BIPED_MODEL::ArmorType::kLightArmor) bipedVisitor->light += count;
                    if (armorType == RE::BIPED_MODEL::ArmorType::kHeavyArmor) bipedVisitor->heavy += count;
                }
                return;
            });
            RE::InventoryChangesAugments::ExecuteAugmentVisitorOnWorn(inventory, &inventoryVisitor);
            return true;
        }
    }// namespace FixSkillLeveling

    namespace RTTIPrinter {
        void Print_RTTI(RE::InventoryChanges::IItemChangeVisitor* target) {
            LogExit exitPrint("RTTIPrinter.Print_RTTI"sv);
            void* object = (void*) target;
            void* vtable = *(void**) object;
            void* info_block = ((void**) vtable)[-1];
            uintptr_t id = ((unsigned long*) info_block)[3];
            uintptr_t info = id + REL::Module::get().base();
            char* name = (char*) info + 16;
            LOG(debug, "vtable = {}, typeinfo = {}, typename = {}", vtable, info_block,
                name);
        }
    }// namespace RTTIPrinter
}// namespace Hooking