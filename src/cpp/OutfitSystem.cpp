#include "OutfitSystem.h"

#include "I18n.h"
#include "NGFeatures.h"
#include "SMP.h"
#include "Utility.h"
#include "compat/ModCompat.h"

#include "RE/REAugments.h"
#include "bindings.h"

#include <algorithm>

#define ERROR_AND_RETURN_EXPR_IF(condition, message, valueExpr, registry, stackId)               \
    if (condition) {                                                                             \
        registry->TraceStack(message, stackId, RE::BSScript::IVirtualMachine::Severity::kError); \
        return (valueExpr);                                                                      \
    }

#define ERROR_AND_RETURN_IF(condition, message, registry, stackId)                               \
    if (condition) {                                                                             \
        registry->TraceStack(message, stackId, RE::BSScript::IVirtualMachine::Severity::kError); \
        return;                                                                                  \
    }

struct bad_name: public std::runtime_error {
    explicit bad_name(const std::string& what_arg) : runtime_error(what_arg){};
};

namespace OutfitSystem {
    std::int32_t GetOutfitNameMaxLength(RE::BSScript::IVirtualMachine* registry,
                                        std::uint32_t stackId,
                                        RE::StaticFunctionTag*) {
        LogExit exitPrint("GetOutfitNameMaxLength"sv);
        return outfit_service_get_singleton_ptr()->inner().max_outfit_name_len();
    }
    std::vector<RE::TESObjectARMO*> GetCarriedArmor(RE::BSScript::IVirtualMachine* registry,
                                                    std::uint32_t stackId,
                                                    RE::StaticFunctionTag*,
                                                    RE::Actor* target) {
        LogExit exitPrint("GetCarriedArmor"sv);
        std::vector<const RE::TESObjectARMO*> result;
        if (target == nullptr) {
            registry->TraceStack("Cannot retrieve data for a None RE::Actor.",
                                 stackId,
                                 RE::BSScript::IVirtualMachine::Severity::kError);
            std::vector<RE::TESObjectARMO*> empty;
            return empty;
        }
        //
        class _Visitor: public RE::IItemChangeVisitorAugment {
            //
            // If the player has a shield equipped, and if we're not overriding that
            // shield, then we need to grab the equipped shield's worn-flags.
            //
        public:
            virtual VisitorReturn Visit(RE::InventoryEntryData* data) override {
                // Return true to continue, or else false to break.
                const auto form = data->object;
                if (form && form->formType == RE::FormType::Armor) {
                    auto armor = skyrim_cast<RE::TESObjectARMO*>(form);
                    if (armor) this->list.push_back(armor);
                }
                return VisitorReturn::kContinue;
            };

            std::vector<const RE::TESObjectARMO*>& list;
            //
            _Visitor(std::vector<const RE::TESObjectARMO*>& l) : list(l){};
        };
        auto inventory = target->GetInventoryChanges();
        if (inventory) {
            _Visitor visitor(result);
            RE::InventoryChangesAugments::ExecuteAugmentVisitor(inventory, &visitor);
        }
        std::vector<RE::TESObjectARMO*> converted_result;
        converted_result.reserve(result.size());
        for (const auto ptr : result) {
            converted_result.push_back(const_cast<RE::TESObjectARMO*>(ptr));
        }
        return converted_result;
    }
    std::vector<RE::TESObjectARMO*> GetWornItems(
        RE::BSScript::IVirtualMachine* registry,
        std::uint32_t stackId,
        RE::StaticFunctionTag*,
        RE::Actor* target) {
        LogExit exitPrint("GetWornItems"sv);
        std::vector<const RE::TESObjectARMO*> result;
        if (target == nullptr) {
            registry->TraceStack("Cannot retrieve data for a None RE::Actor.", stackId, RE::BSScript::IVirtualMachine::Severity::kError);
            std::vector<RE::TESObjectARMO*> empty;
            return empty;
        }
        //
        class _Visitor: public RE::IItemChangeVisitorAugment {
            //
            // If the player has a shield equipped, and if we're not overriding that
            // shield, then we need to grab the equipped shield's worn-flags.
            //
        public:
            virtual VisitorReturn Visit(RE::InventoryEntryData* data) override {
                auto form = data->object;
                if (form && form->formType == RE::FormType::Armor) {
                    auto armor = skyrim_cast<RE::TESObjectARMO*>(form);
                    if (armor) this->list.push_back(armor);
                }
                return VisitorReturn::kContinue;
            };

            std::vector<const RE::TESObjectARMO*>& list;
            //
            _Visitor(std::vector<const RE::TESObjectARMO*>& l) : list(l){};
        };
        auto inventory = target->GetInventoryChanges();
        if (inventory) {
            _Visitor visitor(result);
            RE::InventoryChangesAugments::ExecuteAugmentVisitorOnWorn(inventory, &visitor);
        }
        std::vector<RE::TESObjectARMO*> converted_result;
        converted_result.reserve(result.size());
        for (const auto ptr : result) {
            converted_result.push_back(const_cast<RE::TESObjectARMO*>(ptr));
        }
        return converted_result;
    }
    // Refreshes the armor for the given actor.
    // *It will acquire the AAOS lock at part of the refresh!*
    void refreshArmorFor(RE::Actor* actor) {
        if (!actor) return;
        // An actor without 3D has no biped to rebuild yet. Asking the process
        // manager to update it during the load screen is what used to hang the
        // game, so such actors are skipped and refreshed once the world is up
        // (see SchedulePostLoadRefresh in main.cpp).
        if (!actor->Is3DLoaded()) {
            LOG(debug, "Skipping armor refresh for {}: no 3D yet.", actor->GetDisplayFullName());
            return;
        }
        auto pm = actor->GetActorRuntimeData().currentProcess;
        if (!pm) {
            LOG(debug, "Skipping armor refresh for {}: no AI process yet.", actor->GetDisplayFullName());
            return;
        }
        LOG(debug, "Armor Refresh for {}", actor->GetDisplayFullName());
        {
            //
            // "SetEquipFlag" tells the process manager that the RE::Actor's
            // equipment has changed, and that their ArmorAddons should
            // be updated. If you need to find it in Skyrim Special, you
            // should see a call near the start of EquipManager's func-
            // tion to equip an item.
            //
            // NOTE: AIProcess is also called as RE::ActorProcessManager
            RE::AIProcessAugments::SetEquipFlag(pm, RE::AIProcessAugments::Flag::kUnk01);
            RE::AIProcessAugments::UpdateEquipment(pm, actor);
        }
        // Per-mod follow up: Procedural Armor Damage rescans the biped on its own,
        // this only reports the pieces it cannot attribute to a real armour.
        SOS::Compat::OnOutfitApplied(actor);
        // Faster HDT-SMP keeps its own per-skeleton state; rebuild it for this
        // actor so the new meshes do not inherit the old outfit physics.
        SOS::SMP::ResetActor(actor);
    }
    std::size_t refreshArmorForOutfitUsers(const char* a_outfit) {
        if (!a_outfit || !*a_outfit) return 0;
        std::vector<RE::FormID> wearers;
        {
            // The guard has to live in a named variable: binding inner() from a
            // temporary releases the store lock before the iteration below.
            auto service = outfit_service_get_singleton_ptr();
            wearers = service->inner().actors_wearing(a_outfit);
        }
        std::size_t refreshed = 0;
        for (const auto id : wearers) {
            auto actor = RE::Actor::LookupByID<RE::Actor>(id);
            if (!actor) continue;
            refreshArmorFor(actor);
            ++refreshed;
        }
        return refreshed;
    }
    void refreshArmorForAllConfiguredActors() {
        rust::Vec<std::uint32_t> actors; 
        {
            actors = outfit_service_get_singleton_ptr()->inner().list_actors();
        }
        for (auto& actor_form : actors) {
            auto actor = RE::Actor::LookupByID<RE::Actor>(actor_form);
            if (!actor) continue;
            refreshArmorFor(actor);
        }
    }
    void RefreshArmorFor(RE::BSScript::IVirtualMachine* registry,
                         std::uint32_t stackId,
                         RE::StaticFunctionTag*,
                         RE::Actor* target) {
        LogExit exitPrint("RefreshArmorFor"sv);
        ERROR_AND_RETURN_IF(target == nullptr, "Cannot refresh armor on a None RE::Actor.", registry, stackId);
        refreshArmorFor(target);
    }
    void RefreshArmorForAllConfiguredActors(RE::BSScript::IVirtualMachine* registry,
                                            std::uint32_t stackId,
                                            RE::StaticFunctionTag*) {
        LogExit exitPrint("RefreshArmorForAllConfiguredActors"sv);
        refreshArmorForAllConfiguredActors();
    }

    std::vector<RE::Actor*> ActorsNearPC(RE::BSScript::IVirtualMachine* registry,
                                         std::uint32_t stackId,
                                         RE::StaticFunctionTag*) {
        LogExit exitPrint("RefreshArmorForAllConfiguredActors"sv);
        std::vector<RE::Actor*> result;
        auto pc = RE::PlayerCharacter::GetSingleton();
        ERROR_AND_RETURN_EXPR_IF(pc == nullptr, "Could not get PC Singleton.", result, registry, stackId);
        auto pcCell = pc->GetParentCell();
        ERROR_AND_RETURN_EXPR_IF(pcCell == nullptr, "Could not get cell of PC.", result, registry, stackId);
        result.reserve(pcCell->GetRuntimeData().references.size());
        for (const auto& ref : pcCell->GetRuntimeData().references) {
            RE::TESObjectREFR* objectRefPtr = ref.get();
            auto actorCastedPtr = skyrim_cast<RE::Actor*>(objectRefPtr);
            if (!actorCastedPtr) continue;
            if (!is_form_id_permitted(actorCastedPtr->GetFormID())) continue;
            result.push_back(actorCastedPtr);
        }
        result.shrink_to_fit();
        return result;
    }

    //
    namespace ArmorFormSearchUtils {
        struct SearchData {
            std::vector<std::string> names;
            std::vector<const RE::TESObjectARMO*> armors;
            //
            void setup(std::string nameFilter, bool mustBePlayable) {
                LogExit exitPrint("ArmorFormSearchUtils.setup"sv);
                auto data = RE::TESDataHandler::GetSingleton();
                auto& list = data->GetFormArray(RE::FormType::Armor);
                const auto size = list.size();
                this->names.reserve(size);
                this->armors.reserve(size);
                for (std::uint32_t i = 0; i < size; i++) {
                    const auto form = list[i];
                    if (form && form->formType == RE::FormType::Armor) {
                        auto armor = skyrim_cast<RE::TESObjectARMO*>(form);
                        if (!armor) continue;
                        if (armor->templateArmor)// filter out predefined enchanted variants, to declutter the list
                            continue;
                        if (mustBePlayable && !!(armor->formFlags & RE::TESObjectARMO::RecordFlags::kNonPlayable))
                            continue;
                        std::string armorName;
                        std::string modName;
                        auto file = armor->GetFile();
                        if (file) {
                            modName = std::string(file->GetFilename());
                        }
                        {// get name
                            auto tfn = skyrim_cast<RE::TESFullName*>(armor);
                            if (tfn)
                                armorName = tfn->fullName.data();
                        }
                        if (armorName.empty())// skip nameless armor
                            continue;
                        if (!nameFilter.empty()) {
                            auto itName = std::search(
                                armorName.begin(), armorName.end(),
                                nameFilter.begin(), nameFilter.end(),
                                [](char a, char b) { return toupper(a) == toupper(b); });
                            auto itModName = std::search(
                                modName.begin(), modName.end(),
                                nameFilter.begin(), nameFilter.end(),
                                [](char a, char b) { return toupper(a) == toupper(b); });
                            if (itName == armorName.end() && itModName == modName.end())
                                continue;
                        }
                        this->armors.push_back(armor);
                        this->names.push_back(armorName.c_str());
                    }
                }
            }
            void clear() {
                this->names.clear();
                this->armors.clear();
            }
        };
        static SearchData data;
        //
        //
        void Prep(RE::BSScript::IVirtualMachine* registry,
                  std::uint32_t stackId,
                  RE::StaticFunctionTag*,
                  RE::BSFixedString filter,
                  bool mustBePlayable) {
            LogExit exitPrint("ArmorFormSearchUtils.Prep"sv);
            data.setup(filter.data(), mustBePlayable);
        }
        std::vector<RE::TESObjectARMO*> GetForms(RE::BSScript::IVirtualMachine* registry,
                                                 std::uint32_t stackId,
                                                 RE::StaticFunctionTag*) {
            LogExit exitPrint("ArmorFormSearchUtils.GetForms"sv);
            std::vector<const RE::TESObjectARMO*> result;
            auto& list = data.armors;
            for (auto it = list.begin(); it != list.end(); it++)
                result.push_back(*it);
            std::vector<RE::TESObjectARMO*> converted_result;
            converted_result.reserve(result.size());
            for (const auto ptr : result) {
                converted_result.push_back(const_cast<RE::TESObjectARMO*>(ptr));
            }
            return converted_result;
        }
        std::vector<RE::BSFixedString> GetNames(RE::BSScript::IVirtualMachine* registry,
                                                std::uint32_t stackId,
                                                RE::StaticFunctionTag*) {
            LogExit exitPrint("ArmorFormSearchUtils.GetNames"sv);
            std::vector<RE::BSFixedString> result;
            auto& list = data.names;
            for (auto it = list.begin(); it != list.end(); it++)
                result.push_back(it->c_str());
            return result;
        }
        void Clear(RE::BSScript::IVirtualMachine* registry, std::uint32_t stackId, RE::StaticFunctionTag*) {
            LogExit exitPrint("ArmorFormSearchUtils.Clear"sv);
            data.clear();
        }
        std::unique_ptr<std::vector<TESObjectARMOPtr>> RustSearch(rust::Str query) {
            SearchData data;
            data.setup(std::string(query.data(), query.length()), false);
            std::unique_ptr<std::vector<TESObjectARMOPtr>> results;
            results.reset(new std::vector<TESObjectARMOPtr>(data.armors.size()));
            for (const auto armor : data.armors) {
                results->push_back(TESObjectARMOPtr { armor });
            }
            return results;
        }
    }// namespace ArmorFormSearchUtils
    namespace BodySlotListing {
        enum {
            kBodySlotMin = 30,
            kBodySlotMax = 61,
        };
        static struct {
            std::vector<std::int32_t> bodySlots;
            std::vector<std::string> armorNames;
            std::vector<const RE::TESObjectARMO*> armors;
        } data;
        //
        void Clear(RE::BSScript::IVirtualMachine* registry, std::uint32_t stackId, RE::StaticFunctionTag*) {
            LogExit exitPrint("BodySlotListing.Clear"sv);
            data.bodySlots.clear();
            data.armorNames.clear();
            data.armors.clear();
        }
        void Prep(RE::BSScript::IVirtualMachine* registry,
                  std::uint32_t stackId,
                  RE::StaticFunctionTag*,
                  RE::BSFixedString name) {
            LogExit exitPrint("BodySlotListing.Prep"sv);
            data.bodySlots.clear();
            data.armorNames.clear();
            data.armors.clear();
            //
            auto service = outfit_service_get_singleton_ptr();
            try {
                auto outfit_ptr = service->inner().get_outfit_ptr(name.data());
                if (!outfit_ptr) throw std::out_of_range("");
                auto& outfit = *outfit_ptr;
                auto armors = outfit.armors_c();
                for (std::uint8_t i = kBodySlotMin; i <= kBodySlotMax; i++) {
                    std::uint32_t mask = 1 << (i - kBodySlotMin);
                    for (auto it = armors.begin(); it != armors.end(); it++) {
                        const RE::TESObjectARMO* armor = it->ptr;
                        if (armor && ((armor->GetSlotMask().underlying() & mask) != 0)) {
                            data.bodySlots.push_back(i);
                            data.armors.push_back(armor);
                            {// name
                                auto pFullName = skyrim_cast<const RE::TESFullName*>(armor);
                                if (pFullName)
                                    data.armorNames.emplace_back(pFullName->fullName.data());
                                else
                                    data.armorNames.emplace_back("");
                            }
                        }
                    }
                }
            } catch (std::out_of_range) {
                registry->TraceStack("The specified outfit does not exist.", stackId, RE::BSScript::IVirtualMachine::Severity::kWarning);
            }
        }
        std::vector<RE::TESObjectARMO*> GetArmorForms(RE::BSScript::IVirtualMachine* registry,
                                                      std::uint32_t stackId,
                                                      RE::StaticFunctionTag*) {
            LogExit exitPrint("BodySlotListing.GetArmorForms"sv);
            std::vector<const RE::TESObjectARMO*> result;
            auto& list = data.armors;
            for (auto it = list.begin(); it != list.end(); it++)
                result.push_back(*it);
            std::vector<RE::TESObjectARMO*> converted_result;
            converted_result.reserve(result.size());
            for (const auto ptr : result) {
                converted_result.push_back(const_cast<RE::TESObjectARMO*>(ptr));
            }
            return converted_result;
        }
        std::vector<RE::BSFixedString> GetArmorNames(RE::BSScript::IVirtualMachine* registry,
                                                     std::uint32_t stackId,
                                                     RE::StaticFunctionTag*) {
            LogExit exitPrint("BodySlotListing.GetArmorNames"sv);
            std::vector<RE::BSFixedString> result;
            auto& list = data.armorNames;
            for (auto it = list.begin(); it != list.end(); it++)
                result.push_back(it->c_str());
            return result;
        }
        std::vector<std::int32_t> GetSlotIndices(RE::BSScript::IVirtualMachine* registry,
                                                 std::uint32_t stackId,
                                                 RE::StaticFunctionTag*) {
            LogExit exitPrint("BodySlotListing.GetSlotIndices"sv);
            std::vector<std::int32_t> result;
            auto& list = data.bodySlots;
            for (auto it = list.begin(); it != list.end(); it++)
                result.push_back(*it);
            return result;
        }
    }// namespace BodySlotListing
    namespace BodySlotPolicy {
        std::vector<RE::BSFixedString> BodySlotPolicyNamesForOutfit(RE::BSScript::IVirtualMachine* registry,
                                                                    std::uint32_t stackId,
                                                                    RE::StaticFunctionTag*,
                                                                    RE::BSFixedString name) {
            LogExit exitPrint("BodySlotPolicy.BodySlotPolicyNamesForOutfit"sv);
            std::vector<RE::BSFixedString> result;
            auto service = outfit_service_get_singleton_ptr();
            auto outfit_ptr = service->inner().get_outfit_ptr(name.data());
            if (!outfit_ptr) return std::vector<RE::BSFixedString>(2 * RE::BIPED_OBJECTS_META::kNumSlots, "");
            auto& outfit = *outfit_ptr;
            auto slot_names = outfit.policy_names_for_outfit();
            for (auto& slot_name : slot_names) {
                result.emplace_back(slot_name.c_str());
            }
            return result;
        }
        void SetBodySlotPoliciesForOutfit(RE::BSScript::IVirtualMachine* registry,
                                          std::uint32_t stackId,
                                          RE::StaticFunctionTag*,
                                          RE::BSFixedString name,
                                          std::uint32_t slot,
                                          RE::BSFixedString code) {
            LogExit exitPrint("BodySlotPolicy.SetBodySlotPoliciesForOutfit"sv);
            {
                auto service = outfit_service_get_mut_singleton_ptr();
                auto outfit_ptr = service->inner().get_mut_outfit_ptr(name.data());
                if (!outfit_ptr) return;
                auto& outfit = *outfit_ptr;
                if (slot >= RE::BIPED_OBJECTS_META::kNumSlots) {
                    LOG(err, "Invalid slot {}.", static_cast<std::uint32_t>(slot));
                    return;
                }
                // Empty code means to unset the policy on the slot.
                if (code.c_str() == nullptr || code.empty()) {
                    outfit.set_slot_policy_c(static_cast<RE::BIPED_OBJECT>(slot), nullptr);
                } else {
                    outfit.set_slot_policy_c(static_cast<RE::BIPED_OBJECT>(slot), code.c_str());
                }
            }
            // A policy change only becomes visible in a skinning pass, and a slot
            // that leaves the display set has to be cleared. The lock is gone here
            // because refreshArmorFor runs the equip path (and the hooks with it).
            OutfitSystem::refreshArmorForOutfitUsers(name.data());
        }
        void SetAllBodySlotPoliciesForOutfit(RE::BSScript::IVirtualMachine* registry,
                                             std::uint32_t stackId,
                                             RE::StaticFunctionTag*,
                                             RE::BSFixedString name,
                                             RE::BSFixedString code) {
            LogExit exitPrint("BodySlotPolicy.SetAllBodySlotPoliciesForOutfit"sv);
            {
                auto service = outfit_service_get_mut_singleton_ptr();
                auto outfit_ptr = service->inner().get_mut_outfit_ptr(name.data());
                if (!outfit_ptr) return;
                auto& outfit = *outfit_ptr;
                outfit.set_blanket_slot_policy_c(code.c_str());
            }
            OutfitSystem::refreshArmorForOutfitUsers(name.data());
        }
        void SetBodySlotPolicyToDefaultForOutfit(RE::BSScript::IVirtualMachine* registry,
                                                 std::uint32_t stackId,
                                                 RE::StaticFunctionTag*,
                                                 RE::BSFixedString name) {
            LogExit exitPrint("BodySlotPolicy.SetBodySlotPolicyToDefaultForOutfit"sv);
            {
                auto service = outfit_service_get_mut_singleton_ptr();
                auto outfit_ptr = service->inner().get_mut_outfit_ptr(name.data());
                if (!outfit_ptr) return;
                auto& outfit = *outfit_ptr;
                outfit.reset_to_default_slot_policy();
            }
            OutfitSystem::refreshArmorForOutfitUsers(name.data());
        }
        std::vector<RE::BSFixedString> GetAvailablePolicyNames(RE::BSScript::IVirtualMachine* registry,
                                                               std::uint32_t stackId,
                                                               RE::StaticFunctionTag*) {
            LogExit exitPrint("BodySlotPolicy.GetAvailablePolicyNames"sv);
            auto policies = list_available_policies_c(false);
            std::vector<RE::BSFixedString> result;
            for (const auto& policy : policies) {
                auto key = translation_key_c(policy.name);
                result.emplace_back(key.c_str());
            }
            return result;
        }
        std::vector<std::string> GetAvailablePolicyCodes(RE::BSScript::IVirtualMachine* registry,
                                                               std::uint32_t stackId,
                                                               RE::StaticFunctionTag*) {
            LogExit exitPrint("BodySlotPolicy.GetAvailablePolicyCodes"sv);
            auto policies = list_available_policies_c(false);
            std::vector<std::string> result;
            for (auto& policy : policies) {
                result.emplace_back(policy.name.c_str());
            }
            return result;
        }
    }// namespace BodySlotPolicy
    namespace StringSorts {
        std::vector<RE::BSFixedString> NaturalSort_ASCII(RE::BSScript::IVirtualMachine* registry,
                                                         std::uint32_t stackId,
                                                         RE::StaticFunctionTag*,
                                                         std::vector<RE::BSFixedString> arr,
                                                         bool descending) {
            LogExit exitPrint("StringSorts.NaturalSort_ASCII"sv);
            std::vector<RE::BSFixedString> result = arr;
            std::sort(
                result.begin(),
                result.end(),
                [descending](const RE::BSFixedString& x, const RE::BSFixedString& y) {
                    std::string a(x.data());
                    std::string b(y.data());
                    if (descending)
                        std::swap(a, b);
                    return nat_ord_case_insensitive_c(a, b) > 0;
                });
            return result;
        }

        template<typename T>
        std::vector<T*> NaturalSortPair_ASCII(
            RE::BSScript::IVirtualMachine* registry,
            std::uint32_t stackId,
            RE::StaticFunctionTag*,
            std::vector<RE::BSFixedString> arr,// Array of string
            std::vector<T*> second,            // Array of forms (T)
            bool descending) {
            LogExit exitPrint("StringSorts.NaturalSortPair_ASCII"sv);
            std::size_t size = arr.size();
            if (size != second.size()) {
                registry->TraceStack("The two arrays must be the same length.", stackId, RE::BSScript::IVirtualMachine::Severity::kError);
                return second;
            }
            //
            typedef std::pair<RE::BSFixedString, T*> _pair;
            std::vector<_pair> pairs;
            //
            std::vector<RE::BSFixedString> result;
            {// Copy input array into output array
                result.reserve(size);
                for (std::uint32_t i = 0; i < size; i++) {
                    pairs.emplace_back(arr[i], second[i]);
                }
            }
            std::sort(
                pairs.begin(),
                pairs.end(),
                [descending](const _pair& x, const _pair& y) {
                    std::string a(x.first.data());
                    std::string b(y.first.data());
                    if (descending)
                        std::swap(a, b);
                    return nat_ord_case_insensitive_c(a, b) > 0;
                });
            for (std::uint32_t i = 0; i < size; i++) {
                result.push_back(pairs[i].first);
                second[i] = pairs[i].second;
            }
            return second;
        }
    }// namespace StringSorts
    namespace Utility {
        std::uint32_t HexToInt32(RE::BSScript::IVirtualMachine* registry,
                                 std::uint32_t stackId,
                                 RE::StaticFunctionTag*,
                                 RE::BSFixedString str) {
            LogExit exitPrint("Utility.HexToInt32"sv);
            const char* s = str.data();
            char* discard;
            return strtoul(s, &discard, 16);
        }
        RE::BSFixedString ToHex(RE::BSScript::IVirtualMachine* registry, std::uint32_t stackId, RE::StaticFunctionTag*,

                                std::uint32_t value,
                                std::int32_t length) {
            LogExit exitPrint("Utility.ToHex"sv);
            if (length < 1) {
                registry->TraceStack(
                    "Cannot format a hexadecimal valueinteger to a negative number of digits. Defaulting to eight.",
                    stackId, RE::BSScript::IVirtualMachine::Severity::kWarning);
                length = 8;
            } else if (length > 8) {
                registry->TraceStack("Cannot format a hexadecimal integer longer than eight digits.", stackId, RE::BSScript::IVirtualMachine::Severity::kWarning);
                length = 8;
            }
            char hex[9];
            memset(hex, '0', sizeof(hex));
            hex[length] = '\0';
            while (value > 0 && length--) {
                std::uint8_t digit = value % 0x10;
                value /= 0x10;
                if (digit < 0xA) {
                    hex[length] = digit + '0';
                } else {
                    hex[length] = digit + 0x37;
                }
            }
            return hex;// passes through RE::BSFixedString constructor, which I believe caches the string, so returning local vars should be fine
        }
    }// namespace Utility
    //
    void AddArmorToOutfit(RE::BSScript::IVirtualMachine* registry, std::uint32_t stackId, RE::StaticFunctionTag*,

                          RE::BSFixedString name,
                          RE::TESObjectARMO* armor_skse) {
        LogExit exitPrint("AddArmorToOutfit"sv);
        auto armor = (RE::TESObjectARMO*) (armor_skse);
        ERROR_AND_RETURN_IF(armor == nullptr, "Cannot add a None armor to an outfit.", registry, stackId);
        auto service = outfit_service_get_mut_singleton_ptr();
        try {
            auto outfit_ptr = service->inner().get_mut_outfit_ptr(name.data());
            if (!outfit_ptr) throw std::out_of_range("");
            auto& outfit = *outfit_ptr;
            outfit.insert_armor(armor);
        } catch (std::out_of_range) {
            registry->TraceStack("The specified outfit does not exist.", stackId, RE::BSScript::IVirtualMachine::Severity::kWarning);
        }
    }
    bool ArmorConflictsWithOutfit(RE::BSScript::IVirtualMachine* registry,
                                  std::uint32_t stackId,
                                  RE::StaticFunctionTag*,

                                  RE::TESObjectARMO* armor_skse,
                                  RE::BSFixedString name) {
        LogExit exitPrint("ArmorConflictsWithOutfit"sv);
        auto armor = (RE::TESObjectARMO*) (armor_skse);
        if (armor == nullptr) {
            registry->TraceStack("A None armor can't conflict with anything in an outfit.", stackId, RE::BSScript::IVirtualMachine::Severity::kWarning);
            return false;
        }
        auto service = outfit_service_get_singleton_ptr();
        try {
            auto outfit_ptr = service->inner().get_outfit_ptr(name.data());
            if (!outfit_ptr) throw std::out_of_range("");
            return outfit_ptr->conflicts_with(armor);
        } catch (std::out_of_range) {
            registry->TraceStack("The specified outfit does not exist.", stackId, RE::BSScript::IVirtualMachine::Severity::kWarning);
            return false;
        }
    }
    void CreateOutfit(RE::BSScript::IVirtualMachine* registry,
                      std::uint32_t stackId,
                      RE::StaticFunctionTag*,
                      RE::BSFixedString name) {
        LogExit exitPrint("CreateOutfit"sv);
        auto service = outfit_service_get_mut_singleton_ptr();
        try {
            service->inner().add_outfit(name.data());
        } catch (bad_name) {
            registry->TraceStack("Invalid outfit name specified.", stackId, RE::BSScript::IVirtualMachine::Severity::kError);
            return;
        }
    }
    void DeleteOutfit(RE::BSScript::IVirtualMachine* registry,
                      std::uint32_t stackId,
                      RE::StaticFunctionTag*,
                      RE::BSFixedString name) {
        LogExit exitPrint("DeleteOutfit"sv);
        auto service = outfit_service_get_mut_singleton_ptr();
        if (service->inner().delete_outfit(name.data())) {
            // The deleted outfit may be the one that is being displayed right
            // now; force a refresh so the illusion does not linger.
            CreateRefreshAllArmorsTask();
        }
    }
    std::vector<RE::TESObjectARMO*> GetOutfitContents(RE::BSScript::IVirtualMachine* registry,
                                                      std::uint32_t stackId,
                                                      RE::StaticFunctionTag*,

                                                      RE::BSFixedString name) {
        LogExit exitPrint("GetOutfitContents"sv);
        std::vector<const RE::TESObjectARMO*> result;
        auto service = outfit_service_get_singleton_ptr();
        try {
            auto outfit = service->inner().get_outfit_ptr(name.data());
            if (!outfit) throw std::out_of_range("");
            auto armors = outfit->armors_c();
            for (auto it = armors.begin(); it != armors.end(); ++it)
                result.push_back(it->ptr);
        } catch (std::out_of_range) {
            registry->TraceStack("The specified outfit does not exist.", stackId, RE::BSScript::IVirtualMachine::Severity::kWarning);
        }
        std::vector<RE::TESObjectARMO*> converted_result;
        converted_result.reserve(result.size());
        for (const auto ptr : result) {
            converted_result.push_back(const_cast<RE::TESObjectARMO*>(ptr));
        }
        return converted_result;
    }
    bool GetOutfitFavoriteStatus(RE::BSScript::IVirtualMachine* registry,
                                 std::uint32_t stackId,
                                 RE::StaticFunctionTag*,

                                 RE::BSFixedString name) {
        LogExit exitPrint("GetOutfitFavoriteStatus"sv);
        auto service = outfit_service_get_singleton_ptr();
        bool result = false;
        try {
            auto outfit = service->inner().get_outfit_ptr(name.data());
            if (!outfit) throw std::out_of_range("");
            result = outfit->favorite_c();
        } catch (std::out_of_range) {
            registry->TraceStack("The specified outfit does not exist.", stackId, RE::BSScript::IVirtualMachine::Severity::kWarning);
        }
        return result;
    }
    RE::BSFixedString GetSelectedOutfit(RE::BSScript::IVirtualMachine* registry,
                                        std::uint32_t stackId,
                                        RE::StaticFunctionTag*,
                                        RE::Actor* actor) {
        LogExit exitPrint("GetSelectedOutfit"sv);
        if (!actor)
            return RE::BSFixedString("");
        auto service = outfit_service_get_singleton_ptr();
        auto outfit = service->inner().current_outfit_ptr(actor->GetFormID());
        if (!outfit) return "";
        return outfit->name_c().c_str();
    }
    bool IsEnabled(RE::BSScript::IVirtualMachine* registry, std::uint32_t stackId, RE::StaticFunctionTag*) {
        LogExit exitPrint("IsEnabled"sv);
        auto service = outfit_service_get_singleton_ptr();
        return service->inner().enabled_c();
    }
    std::vector<RE::BSFixedString> ListOutfits(RE::BSScript::IVirtualMachine* registry,
                                               std::uint32_t stackId,
                                               RE::StaticFunctionTag*,

                                               bool favoritesOnly) {
        LogExit exitPrint("ListOutfits"sv);
        auto service = outfit_service_get_singleton_ptr();
        std::vector<RE::BSFixedString> result;
        auto names = service->inner().get_outfit_names(favoritesOnly);
        result.reserve(names.size());
        for (auto it = names.begin(); it != names.end(); ++it)
            result.push_back(it->c_str());
        return result;
    }
    void RemoveArmorFromOutfit(RE::BSScript::IVirtualMachine* registry, std::uint32_t stackId, RE::StaticFunctionTag*,

                               RE::BSFixedString name,
                               RE::TESObjectARMO* armor_skse) {
        LogExit exitPrint("RemoveArmorFromOutfit"sv);
        auto armor = (RE::TESObjectARMO*) (armor_skse);
        ERROR_AND_RETURN_IF(armor == nullptr, "Cannot remove a None armor from an outfit.", registry, stackId);
        auto service = outfit_service_get_mut_singleton_ptr();
        try {
            auto outfit = service->inner().get_mut_outfit_ptr(name.data());
            if (!outfit) throw std::out_of_range("");
            outfit->erase_armor(armor);
        } catch (std::out_of_range) {
            registry->TraceStack("The specified outfit does not exist.", stackId, RE::BSScript::IVirtualMachine::Severity::kWarning);
        }
    }
    void RemoveConflictingArmorsFrom(RE::BSScript::IVirtualMachine* registry,
                                     std::uint32_t stackId,
                                     RE::StaticFunctionTag*,

                                     RE::TESObjectARMO* armor_skse,
                                     RE::BSFixedString name) {
        LogExit exitPrint("RemoveConflictingArmorsFrom"sv);
        auto armor = (RE::TESObjectARMO*) (armor_skse);
        ERROR_AND_RETURN_IF(armor == nullptr,
                            "A None armor can't conflict with anything in an outfit.",
                            registry,
                            stackId);
        auto service = outfit_service_get_mut_singleton_ptr();
        try {
            auto outfit = service->inner().get_mut_outfit_ptr(name.data());
            if (!outfit) throw std::out_of_range("");
            auto armors = outfit->armors_c();
            std::vector<const RE::TESObjectARMO*> conflicts;
            const auto candidateMask = armor->GetSlotMask().underlying();
            for (auto it = armors.begin(); it != armors.end(); ++it) {
                const RE::TESObjectARMO* existing = it->ptr;
                if (existing && (existing->GetSlotMask().underlying() & candidateMask) != 0)
                    conflicts.push_back(existing);
            }
            for (auto it = conflicts.begin(); it != conflicts.end(); ++it)
                outfit->erase_armor(*it);
        } catch (std::out_of_range) {
            registry->TraceStack("The specified outfit does not exist.", stackId, RE::BSScript::IVirtualMachine::Severity::kError);
            return;
        }
    }
    bool RenameOutfit(RE::BSScript::IVirtualMachine* registry, std::uint32_t stackId, RE::StaticFunctionTag*,

                      RE::BSFixedString name,
                      RE::BSFixedString changeTo) {
        LogExit exitPrint("RenameOutfit"sv);
        auto service = outfit_service_get_mut_singleton_ptr();
        auto outcome = service->inner().rename_outfit(name.data(), changeTo.data());
        if (outcome == 2) {
            registry->TraceStack("The desired name is taken.", stackId, RE::BSScript::IVirtualMachine::Severity::kError);
            return false;
        } else if (outcome == 1) {
            registry->TraceStack("The specified outfit does not exist.", stackId, RE::BSScript::IVirtualMachine::Severity::kError);
            return false;
        } else if (outcome != 0) {
            registry->TraceStack("Unknown error during rename.", stackId, RE::BSScript::IVirtualMachine::Severity::kError);
            return false;
        }
        return true;
    }
    void SetOutfitFavoriteStatus(RE::BSScript::IVirtualMachine* registry,
                                 std::uint32_t stackId,
                                 RE::StaticFunctionTag*,

                                 RE::BSFixedString name,
                                 bool favorite) {
        LogExit exitPrint("SetOutfitFavoriteStatus"sv);
        auto service = outfit_service_get_mut_singleton_ptr();
        service->inner().set_favorite(name.data(), favorite);
    }
    bool OutfitExists(RE::BSScript::IVirtualMachine* registry, std::uint32_t stackId, RE::StaticFunctionTag*,

                      RE::BSFixedString name) {
        LogExit exitPrint("OutfitExists"sv);
        auto service = outfit_service_get_singleton_ptr();
        return service->inner().has_outfit(name.data());
    }
    void OverwriteOutfit(RE::BSScript::IVirtualMachine* registry,
                         std::uint32_t stackId,
                         RE::StaticFunctionTag*,
                         RE::BSFixedString name,
                         std::vector<RE::TESObjectARMO*> armors) {
        LogExit exitPrint("OverwriteOutfit"sv);
        auto service = outfit_service_get_mut_singleton_ptr();
        try {
            std::vector<const RE::TESObjectARMO*> pieces;
            pieces.reserve(armors.size());
            for (auto* armor : armors) {
                if (armor) {
                    pieces.push_back(armor);
                }
            }
            if (service->inner().replace_outfit_armors(name.data(), pieces, true)) {
                // Overwriting the active outfit has to update the illusion and
                // bump the revision the menus cache on.
                CreateRefreshAllArmorsTask();
            }
        } catch (bad_name) {
            registry->TraceStack("Invalid outfit name specified.", stackId, RE::BSScript::IVirtualMachine::Severity::kError);
            return;
        }
    }
    void SetEnabled(RE::BSScript::IVirtualMachine* registry,
                    std::uint32_t stackId,
                    RE::StaticFunctionTag*,
                    bool state) {
        LogExit exitPrint("SetEnabled"sv);
        auto service = outfit_service_get_mut_singleton_ptr();
        service->inner().set_enabled(state);
    }
    void SetSelectedOutfit(RE::BSScript::IVirtualMachine* registry,
                           std::uint32_t stackId,
                           RE::StaticFunctionTag*,
                           RE::Actor* actor,
                           RE::BSFixedString name) {
        LogExit exitPrint("SetSelectedOutfit"sv);
        if (!actor)
            return;
        auto service = outfit_service_get_mut_singleton_ptr();
        service->inner().set_outfit_c(name.data(), actor->GetFormID());
    }
    void AddActor(RE::BSScript::IVirtualMachine* registry,
                  std::uint32_t stackId,
                  RE::StaticFunctionTag*,
                  RE::Actor* target) {
        LogExit exitPrint("AddActor"sv);
        if (!target) {
            registry->TraceStack("Cannot add a None RE::Actor.", stackId, RE::BSScript::IVirtualMachine::Severity::kError);
            return;
        }
        auto service = outfit_service_get_mut_singleton_ptr();
        service->inner().add_actor(target->GetFormID());
    }
    void RemoveActor(RE::BSScript::IVirtualMachine* registry,
                     std::uint32_t stackId,
                     RE::StaticFunctionTag*,
                     RE::Actor* target) {
        LogExit exitPrint("RemoveActor"sv);
        if (!target) {
            registry->TraceStack("Cannot remove a None RE::Actor.", stackId, RE::BSScript::IVirtualMachine::Severity::kError);
            return;
        }
        auto service = outfit_service_get_mut_singleton_ptr();
        service->inner().remove_actor(target->GetFormID());
    }
    std::vector<RE::Actor*> ListActors(RE::BSScript::IVirtualMachine* registry,
                                       std::uint32_t stackId,
                                       RE::StaticFunctionTag*) {
        LogExit exitPrint("ListActors"sv);
        auto service = outfit_service_get_singleton_ptr();
        auto actors = service->inner().list_actors();
        std::vector<RE::Actor*> actorVec;
        for (auto& actor_form : actors) {
            auto actor = RE::Actor::LookupByID<RE::Actor>(actor_form);
            if (!actor) continue;
#if _DEBUG
            LOG(debug, "INNER: Actor {} has refcount {}", actor->GetDisplayFullName(), actor->QRefCount());
#endif
            if (actor->QRefCount() == 1) {
                LOG(warn, "ListActors will return an actor {} with refcount of 1. This may crash.", actor->GetDisplayFullName());
            }
            actorVec.push_back(actor);
        }
        std::sort(
            actorVec.begin(),
            actorVec.end(),
            [](const RE::Actor* x, const RE::Actor* y) {
                return x < y;
            });
#if _DEBUG
        for (const auto& actor : actorVec) {
            LOG(debug, "Actor {} has refcount {}", actor->GetDisplayFullName(), actor->QRefCount());
        }
#endif
        return actorVec;
    }
    void SetLocationBasedAutoSwitchEnabled(RE::BSScript::IVirtualMachine* registry,
                                           std::uint32_t stackId,
                                           RE::StaticFunctionTag*,
                                           RE::Actor* actor,
                                           bool value) {
        LogExit exitPrint("SetLocationBasedAutoSwitchEnabled"sv);
        if (!actor) return;
        outfit_service_get_mut_singleton_ptr()->inner().set_state_based_switching_enabled(value, actor->GetFormID());
    }
    bool GetLocationBasedAutoSwitchEnabled(RE::BSScript::IVirtualMachine* registry,
                                           std::uint32_t stackId,
                                           RE::StaticFunctionTag*,
                                           RE::Actor* actor) {
        LogExit exitPrint("GetLocationBasedAutoSwitchEnabled"sv);
        if (!actor) return false;
        return outfit_service_get_singleton_ptr()->inner().get_state_based_switching_enabled_c(actor->GetFormID());
    }
    std::vector<std::uint32_t> GetAutoSwitchStateArray(RE::BSScript::IVirtualMachine* registry,
                                                          std::uint32_t stackId,
                                                          RE::StaticFunctionTag*) {
        LogExit exitPrint("GetAutoSwitchStateArray"sv);
        std::vector<std::uint32_t> result;
        for (StateType i : {
                 StateType::Combat,
                 StateType::World,
                 StateType::WorldSnowy,
                 StateType::WorldRainy,
                 StateType::City,
                 StateType::CitySnowy,
                 StateType::CityRainy,
                 StateType::Town,
                 StateType::TownSnowy,
                 StateType::TownRainy,
                 StateType::Dungeon,
                 StateType::DungeonSnowy,
                 StateType::DungeonRainy}) {
            result.push_back(std::uint32_t(i));
        }
        return result;
    }
    std::optional<StateType> identifyStateType(const OutfitService& service, RE::Actor* actor) {
        LogExit exitPrint("identifyStateType"sv);
        if (!actor) return std::nullopt;
        auto is_in_combat = actor->IsInCombat();
        auto location = actor->GetCurrentLocation();
        auto sky = RE::Sky::GetSingleton();

        // Collect weather information.
        WeatherFlags weather_flags;
        if (sky) {
            weather_flags.snowy = sky->IsSnowing();
            weather_flags.rainy = sky->IsRaining();
        }
        LOG(debug, "Identifying state for {}: rain: {}, snow: {}, loc: {:x}, combat: {}", actor->GetDisplayFullName(), weather_flags.rainy, weather_flags.snowy, (uintptr_t) location, is_in_combat);
        // Collect location keywords
        rust::Vec<rust::String> keywords;
        keywords.reserve(20);
        std::uint32_t depth = 0;
        while (location && depth++ < 64) {
            const std::uint32_t max = location->GetNumKeywords();
            for (std::uint32_t i = 0; i < max; i++) {
                // GetKeywordAt returns an empty optional for gaps in the keyword
                // array; calling value() there would throw inside a native call.
                const auto keyword = location->GetKeywordAt(i);
                if (!keyword || !*keyword) {
                    continue;
                }
                const char* editorId = (*keyword)->GetFormEditorID();
                if (editorId && *editorId) {
                    keywords.push_back(rust::String(editorId));
                }
            }
            location = location->parentLoc;
        }
        auto result = service.check_location_type_c(keywords, weather_flags, is_in_combat, actor->GetFormID());
        if (result.has_value) {
            return result.value;
        } else {
            return std::nullopt;
        }
    }
    std::uint32_t IdentifyStateType(RE::BSScript::IVirtualMachine* registry,
                                    std::uint32_t stackId,
                                    RE::StaticFunctionTag*,
                                    RE::Actor* actor) {
        LogExit exitPrint("IdentifyStateType"sv);
        // NOTE: Identify the location for Papyrus. In the event no location is identified, we lie to Papyrus and say "World".
        //       Therefore, Papyrus cannot assume that locations returned have an outfit assigned, at least not for "World".
        auto service = outfit_service_get_singleton_ptr();
        if (!actor) return static_cast<std::uint32_t>(StateType::World);
        return static_cast<std::uint32_t>(identifyStateType(service->inner(), actor).value_or(StateType::World));
    }
    bool setOutfitByName(OutfitService& service, RE::Actor* actor, const char* name) {
        LogExit exitPrint("setOutfitByName"sv);
        if (!actor || !name || *name == '\0') return false;
        if (!service.has_outfit(name)) return false;
        // Note: re-applying the current outfit still bumps the revision in the
        // service, so the illusion is rebuilt instead of a stale clone surviving.
        service.set_outfit_c(name, actor->GetFormID());
        return true;
    }
    void setOutfitUsingState(OutfitService& service, RE::Actor* actor) {
        LogExit exitPrint("setOutfitUsingState"sv);
        // NOTE: Location can be NULL.
        if (!actor) return;
        if (service.get_state_based_switching_enabled_c(actor->GetFormID())) {
            auto state = identifyStateType(service, actor);
            // Debug notifications for location classification.
            /*
            const char* locationName = StateTypeStrings[static_cast<std::uint32_t>(location)];
            char message[100];
            sprintf_s(message, "SOS: This location is a %s.", locationName);
            RE::SendHUDMessage::ShowHUDMessage(message, nullptr, false);
            */
            if (state.has_value()) {
                service.set_outfit_using_state(state.value(), actor->GetFormID());
            }
        }
    }
    void SetOutfitUsingState(RE::BSScript::IVirtualMachine* registry, std::uint32_t stackId, RE::StaticFunctionTag*, RE::Actor* actor) {
        LogExit exitPrint("SetOutfitUsingState"sv);
        // NOTE: Location can be NULL.
        auto service = outfit_service_get_mut_singleton_ptr();
        if (!actor) return;
        setOutfitUsingState(service->inner(), actor);
    }
    void setOutfitUsingStateForAllConfiguredActors(OutfitService& service) {
        LogExit exitPrint("setOutfitUsingStateForAllConfiguredActors"sv);
        rust::Vec<std::uint32_t> actors = service.list_actors(); 
        for (auto& actor_form : actors) {
            auto actor = RE::Actor::LookupByID<RE::Actor>(actor_form);
            if (!actor) continue;
            setOutfitUsingState(service, actor);
        }
    }
    void SetOutfitUsingStateForAllConfiguredActors(RE::BSScript::IVirtualMachine* registry,
                                            std::uint32_t stackId,
                                            RE::StaticFunctionTag*) {
        LogExit exitPrint("SetOutfitUsingStateForAllConfiguredActors"sv);
        auto service = outfit_service_get_mut_singleton_ptr();
        setOutfitUsingStateForAllConfiguredActors(service->inner());
    }
    void NotifyCombatStateChanged(RE::BSScript::IVirtualMachine* registry,
                                    std::uint32_t stackId,
                                    RE::StaticFunctionTag*,
                                    RE::Actor* actor) {
        LogExit exitPrint("NotifyCombatStateChanged"sv);
        if (!actor) return;
        {
            LOG(debug, "Script notification of combat change for {}", actor->GetDisplayFullName());
            OutfitSystem::setOutfitUsingState(outfit_service_get_mut_singleton_ptr()->inner(), actor);
        }
        OutfitSystem::refreshArmorFor(actor);
    }
    void SetStateOutfit(RE::BSScript::IVirtualMachine* registry, std::uint32_t stackId, RE::StaticFunctionTag*,
                           RE::Actor* actor,
                           std::uint32_t location,
                           RE::BSFixedString name) {
        LogExit exitPrint("SetStateOutfit"sv);
        if (!actor)
            return;
        if (strcmp(name.data(), "") == 0) {
            // Location outfit assignment is never allowed to be empty string. Use unset instead.
            return;
        }
        outfit_service_get_mut_singleton_ptr()->inner().set_state_outfit(StateType(location), actor->GetFormID(), name.data());
    }
    void UnsetStateOutfit(RE::BSScript::IVirtualMachine* registry, std::uint32_t stackId, RE::StaticFunctionTag*,
                             RE::Actor* actor,
                             std::uint32_t location) {
        LogExit exitPrint("UnsetStateOutfit"sv);
        if (!actor)
            return;
        return outfit_service_get_mut_singleton_ptr()->inner().unset_state_outfit(StateType(location), actor->GetFormID());
    }
    RE::BSFixedString GetStateOutfit(RE::BSScript::IVirtualMachine* registry,
                                        std::uint32_t stackId,
                                        RE::StaticFunctionTag*,
                                        RE::Actor* actor,
                                        std::uint32_t location) {
        LogExit exitPrint("GetStateOutfit"sv);
        if (!actor)
            return RE::BSFixedString("");
        auto outfit = outfit_service_get_singleton_ptr()->inner().get_state_outfit_name_c(StateType(location), actor->GetFormID());
        if (!outfit.empty()) {
            return RE::BSFixedString(outfit.c_str());
        } else {
            // Empty string means "no outfit assigned" for this location type.
            return RE::BSFixedString("");
        }
    }
    bool ExportSettings(RE::BSScript::IVirtualMachine*, std::uint32_t, RE::StaticFunctionTag*) {
        LogExit exitPrint("ExportSettings"sv);
        std::string message;
        const bool ok = SOS::Features::ExportOutfitsToJson(message);
        RE::SendHUDMessage::ShowHUDMessage(message.c_str(), nullptr, false);
        return ok;
    }
    bool ImportSettings(RE::BSScript::IVirtualMachine*, std::uint32_t, RE::StaticFunctionTag*) {
        LogExit exitPrint("ImportSettings"sv);
        std::string message;
        const bool ok = SOS::Features::ImportOutfitsFromJson(message);
        RE::SendHUDMessage::ShowHUDMessage(message.c_str(), nullptr, false);
        if (ok) {
            CreateRefreshAllArmorsTask();
        }
        return ok;
    }
}// namespace OutfitSystem

bool OutfitSystem::RegisterPapyrus(RE::BSScript::IVirtualMachine* registry) {
    registry->RegisterFunction(
        "GetOutfitNameMaxLength",
        "SkyrimOutfitSystemNativeFuncs",
        GetOutfitNameMaxLength,
        true);
    registry->RegisterFunction(
        "GetCarriedArmor",
        "SkyrimOutfitSystemNativeFuncs",
        GetCarriedArmor);
    registry->RegisterFunction(
        "GetWornItems",
        "SkyrimOutfitSystemNativeFuncs",
        GetWornItems);
    registry->RegisterFunction(
        "RefreshArmorFor",
        "SkyrimOutfitSystemNativeFuncs",
        RefreshArmorFor);
    registry->RegisterFunction(
        "RefreshArmorForAllConfiguredActors",
        "SkyrimOutfitSystemNativeFuncs",
        RefreshArmorForAllConfiguredActors);
    registry->RegisterFunction(
        "ActorNearPC",
        "SkyrimOutfitSystemNativeFuncs",
        ActorsNearPC);
    //
    {// armor form search utils
        registry->RegisterFunction(
            "PrepArmorSearch",
            "SkyrimOutfitSystemNativeFuncs",
            ArmorFormSearchUtils::Prep);
        registry->RegisterFunction(
            "GetArmorSearchResultForms",
            "SkyrimOutfitSystemNativeFuncs",
            ArmorFormSearchUtils::GetForms);
        registry->RegisterFunction(
            "GetArmorSearchResultNames",
            "SkyrimOutfitSystemNativeFuncs",
            ArmorFormSearchUtils::GetNames);
        registry->RegisterFunction(
            "ClearArmorSearch",
            "SkyrimOutfitSystemNativeFuncs",
            ArmorFormSearchUtils::Clear);
    }
    {// body slot data
        registry->RegisterFunction(
            "PrepOutfitBodySlotListing",
            "SkyrimOutfitSystemNativeFuncs",
            BodySlotListing::Prep);
        registry->RegisterFunction(
            "GetOutfitBodySlotListingArmorForms",
            "SkyrimOutfitSystemNativeFuncs",
            BodySlotListing::GetArmorForms);
        registry->RegisterFunction(
            "GetOutfitBodySlotListingArmorNames",
            "SkyrimOutfitSystemNativeFuncs",
            BodySlotListing::GetArmorNames);
        registry->RegisterFunction(
            "GetOutfitBodySlotListingSlotIndices",
            "SkyrimOutfitSystemNativeFuncs",
            BodySlotListing::GetSlotIndices);
        registry->RegisterFunction(
            "ClearOutfitBodySlotListing",
            "SkyrimOutfitSystemNativeFuncs",
            BodySlotListing::Clear);
    }
    {//body slot policy
        registry->RegisterFunction(
            "BodySlotPolicyNamesForOutfit",
            "SkyrimOutfitSystemNativeFuncs",
            BodySlotPolicy::BodySlotPolicyNamesForOutfit);
        registry->RegisterFunction(
            "SetBodySlotPoliciesForOutfit",
            "SkyrimOutfitSystemNativeFuncs",
            BodySlotPolicy::SetBodySlotPoliciesForOutfit);
        registry->RegisterFunction(
            "SetAllBodySlotPoliciesForOutfit",
            "SkyrimOutfitSystemNativeFuncs",
            BodySlotPolicy::SetAllBodySlotPoliciesForOutfit);
        registry->RegisterFunction(
            "SetBodySlotPolicyToDefaultForOutfit",
            "SkyrimOutfitSystemNativeFuncs",
            BodySlotPolicy::SetBodySlotPolicyToDefaultForOutfit);
        registry->RegisterFunction(
            "GetAvailablePolicyNames",
            "SkyrimOutfitSystemNativeFuncs",
            BodySlotPolicy::GetAvailablePolicyNames);
        registry->RegisterFunction(
            "GetAvailablePolicyCodes",
            "SkyrimOutfitSystemNativeFuncs",
            BodySlotPolicy::GetAvailablePolicyCodes);
    }
    {// string sorts
        registry->RegisterFunction(
            "NaturalSort_ASCII",
            "SkyrimOutfitSystemNativeFuncs",
            StringSorts::NaturalSort_ASCII,
            true);
        registry->RegisterFunction(
            "NaturalSortPairArmor_ASCII",
            "SkyrimOutfitSystemNativeFuncs",
            StringSorts::NaturalSortPair_ASCII<RE::TESObjectARMO>,
            true);
    }
    {// Utility
        registry->RegisterFunction(
            "HexToInt32",
            "SkyrimOutfitSystemNativeFuncs",
            Utility::HexToInt32,
            true);
        registry->RegisterFunction(
            "ToHex",
            "SkyrimOutfitSystemNativeFuncs",
            Utility::ToHex,
            true);
    }
    //
    registry->RegisterFunction(
        "AddArmorToOutfit",
        "SkyrimOutfitSystemNativeFuncs",
        AddArmorToOutfit);
    registry->RegisterFunction(
        "ArmorConflictsWithOutfit",
        "SkyrimOutfitSystemNativeFuncs",
        ArmorConflictsWithOutfit);
    registry->RegisterFunction(
        "CreateOutfit",
        "SkyrimOutfitSystemNativeFuncs",
        CreateOutfit);
    registry->RegisterFunction(
        "DeleteOutfit",
        "SkyrimOutfitSystemNativeFuncs",
        DeleteOutfit);
    registry->RegisterFunction(
        "GetOutfitContents",
        "SkyrimOutfitSystemNativeFuncs",
        GetOutfitContents);
    registry->RegisterFunction(
        "GetOutfitFavoriteStatus",
        "SkyrimOutfitSystemNativeFuncs",
        GetOutfitFavoriteStatus);
    registry->RegisterFunction(
        "SetOutfitFavoriteStatus",
        "SkyrimOutfitSystemNativeFuncs",
        SetOutfitFavoriteStatus);
    registry->RegisterFunction(
        "IsEnabled",
        "SkyrimOutfitSystemNativeFuncs",
        IsEnabled);
    registry->RegisterFunction(
        "GetSelectedOutfit",
        "SkyrimOutfitSystemNativeFuncs",
        GetSelectedOutfit);
    registry->RegisterFunction(
        "ListOutfits",
        "SkyrimOutfitSystemNativeFuncs",
        ListOutfits);
    registry->RegisterFunction(
        "RemoveArmorFromOutfit",
        "SkyrimOutfitSystemNativeFuncs",
        RemoveArmorFromOutfit);
    registry->RegisterFunction(
        "RemoveConflictingArmorsFrom",
        "SkyrimOutfitSystemNativeFuncs",
        RemoveConflictingArmorsFrom);
    registry->RegisterFunction(
        "RenameOutfit",
        "SkyrimOutfitSystemNativeFuncs",
        RenameOutfit);
    registry->RegisterFunction(
        "OutfitExists",
        "SkyrimOutfitSystemNativeFuncs",
        OutfitExists);
    registry->RegisterFunction(
        "OverwriteOutfit",
        "SkyrimOutfitSystemNativeFuncs",
        OverwriteOutfit);
    registry->RegisterFunction(
        "SetEnabled",
        "SkyrimOutfitSystemNativeFuncs",
        SetEnabled);
    registry->RegisterFunction(
        "SetSelectedOutfit",
        "SkyrimOutfitSystemNativeFuncs",
        SetSelectedOutfit);
    registry->RegisterFunction(
        "AddActor",
        "SkyrimOutfitSystemNativeFuncs",
        AddActor);
    registry->RegisterFunction(
        "RemoveActor",
        "SkyrimOutfitSystemNativeFuncs",
        RemoveActor);
    registry->RegisterFunction(
        "ListActors",
        "SkyrimOutfitSystemNativeFuncs",
        ListActors);
    registry->RegisterFunction(
        "SetLocationBasedAutoSwitchEnabled",
        "SkyrimOutfitSystemNativeFuncs",
        SetLocationBasedAutoSwitchEnabled);
    registry->RegisterFunction(
        "GetLocationBasedAutoSwitchEnabled",
        "SkyrimOutfitSystemNativeFuncs",
        GetLocationBasedAutoSwitchEnabled);
    registry->RegisterFunction(
        "GetAutoSwitchStateArray",
        "SkyrimOutfitSystemNativeFuncs",
        GetAutoSwitchStateArray);
    registry->RegisterFunction(
        "IdentifyStateType",
        "SkyrimOutfitSystemNativeFuncs",
        IdentifyStateType);
    registry->RegisterFunction(
        "SetOutfitUsingState",
        "SkyrimOutfitSystemNativeFuncs",
        SetOutfitUsingState);
    registry->RegisterFunction(
        "SetOutfitUsingStateForAllConfiguredActors",
        "SkyrimOutfitSystemNativeFuncs",
        SetOutfitUsingStateForAllConfiguredActors);
    registry->RegisterFunction(
        "NotifyCombatStateChanged",
        "SkyrimOutfitSystemNativeFuncs",
        NotifyCombatStateChanged);
    registry->RegisterFunction(
        "SetStateOutfit",
        "SkyrimOutfitSystemNativeFuncs",
        SetStateOutfit);
    registry->RegisterFunction(
        "UnsetStateOutfit",
        "SkyrimOutfitSystemNativeFuncs",
        UnsetStateOutfit);
    registry->RegisterFunction(
        "GetStateOutfit",
        "SkyrimOutfitSystemNativeFuncs",
        GetStateOutfit);
    registry->RegisterFunction(
        "ExportSettings",
        "SkyrimOutfitSystemNativeFuncs",
        ExportSettings);
    registry->RegisterFunction(
        "ImportSettings",
        "SkyrimOutfitSystemNativeFuncs",
        ImportSettings);

    // NG additions: random outfit selection and outfit enumeration/search.
    registry->RegisterFunction(
        "RandomOutfit",
        "SkyrimOutfitSystemNativeFuncs",
        SOS::Features::RandomOutfit);
    registry->RegisterFunction(
        "GetOutfitCount",
        "SkyrimOutfitSystemNativeFuncs",
        SOS::Features::GetOutfitCount);
    registry->RegisterFunction(
        "GetOutfitNameByIndex",
        "SkyrimOutfitSystemNativeFuncs",
        SOS::Features::GetOutfitNameByIndex);
    registry->RegisterFunction(
        "SearchOutfits",
        "SkyrimOutfitSystemNativeFuncs",
        SOS::Features::SearchOutfits);

    // Quick picker (the "Change Outfit" lesser power opens the popup window).
    registry->RegisterFunction(
        "OpenQuickPicker",
        "SkyrimOutfitSystemNativeFuncs",
        SOS::Features::OpenQuickPicker);
    registry->RegisterFunction(
        "CloseQuickPicker",
        "SkyrimOutfitSystemNativeFuncs",
        SOS::Features::CloseQuickPicker);
    registry->RegisterFunction(
        "IsQuickPickerVisible",
        "SkyrimOutfitSystemNativeFuncs",
        SOS::Features::IsQuickPickerVisible);
    registry->RegisterFunction(
        "IsQuickSlotRandom",
        "SkyrimOutfitSystemNativeFuncs",
        SOS::Features::IsQuickSlotRandom);
    registry->RegisterFunction(
        "SetQuickSlotRandom",
        "SkyrimOutfitSystemNativeFuncs",
        SOS::Features::SetQuickSlotRandom);

    return true;
}

// Event Subscriptions

class CombatStartSubscription: public RE::BSTEventSink<RE::TESCombatEvent>, public RE::BSTEventSink<RE::TESDeathEvent> {
    RE::BSEventNotifyControl ProcessEvent(const RE::TESCombatEvent* a_event, RE::BSTEventSource<RE::TESCombatEvent>* a_eventSource) override {
        if (!a_event) return RE::BSEventNotifyControl::kContinue;
        if (a_event->actor.get() && a_event->targetActor.get()) {
            LOG(debug, "Combat state {}: {} -> {}", a_event->newState.underlying(), a_event->actor->GetDisplayFullName(), a_event->targetActor->GetDisplayFullName());
        } else {
            LOG(debug, "Combat state {}: {:x} -> {:x}", a_event->newState.underlying(), (uintptr_t)a_event->actor.get(), (uintptr_t)a_event->targetActor.get());
        }
        std::array<RE::Actor*, 3> relevantActors{nullptr};
        {
            auto service = outfit_service_get_mut_singleton_ptr();
            auto& store = service->inner();
            const auto eligible = [&store](RE::FormID a_actor) {
                // Honor the per-actor switch and allow state-only setups (no base
                // outfit). The old check only looked at should_override(), so a
                // disabled switch still reactivated state outfits on combat.
                return store.enabled_c() && store.get_state_based_switching_enabled_c(a_actor) &&
                       (store.should_override(a_actor) || store.has_any_state_outfit(a_actor));
            };
            if (a_event->actor.get() && eligible(a_event->actor->GetFormID())) {
                relevantActors[0] = a_event->actor->As<RE::Actor>();
            }
            if (a_event->targetActor.get() && eligible(a_event->targetActor->GetFormID())) {
                relevantActors[1] = a_event->targetActor->As<RE::Actor>();
            }
            // For some reason, we don't get notifications about the PC ending combat, so we always check the PC.
            if (auto* player = RE::PlayerCharacter::GetSingleton(); player && eligible(player->GetFormID())) {
                relevantActors[2] = player;
            }
            for (auto actor : relevantActors) {
                if (actor) {
                    LOG(debug, "New combat state for {} ({}): {}", actor->GetDisplayFullName(), (uintptr_t) actor, a_event->newState.underlying());
                    OutfitSystem::setOutfitUsingState(service->inner(), actor);
                }
            }
        }
        // This last step requires the AAOS lock to be released
        for (auto actor : relevantActors) {
            if (actor) {
                OutfitSystem::refreshArmorFor(actor);
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>* a_eventSource) override {
        LOG(debug, "Character died. Checking for PC combat state");
        bool shouldOverride = false;
        auto* player = RE::PlayerCharacter::GetSingleton();
        {
            auto service = outfit_service_get_mut_singleton_ptr();
            if (player) {
                auto& store = service->inner();
                shouldOverride = store.enabled_c() && store.get_state_based_switching_enabled_c(player->GetFormID()) &&
                                  (store.should_override(player->GetFormID()) || store.has_any_state_outfit(player->GetFormID()));
                if (shouldOverride) {
                    OutfitSystem::setOutfitUsingState(store, player);
                }
            }
        }
        if (shouldOverride && player) {
            OutfitSystem::refreshArmorFor(player);
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

bool OutfitSystem::RegisterEvents() {
    auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
    if (!holder) {
        SKSE::log::error("Events: ScriptEventSourceHolder is unavailable; combat/death outfit switching stays disabled.");
        return false;
    }
    auto* combatSubscription = new CombatStartSubscription();
    holder->AddEventSink<RE::TESCombatEvent>(combatSubscription);
    holder->AddEventSink<RE::TESDeathEvent>(combatSubscription);
    return true;
}