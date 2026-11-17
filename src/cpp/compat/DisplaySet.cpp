#include "compat/DisplaySet.h"

#include "GameState.h"

#include "core/OutfitService.h"
#include "core/Settings.h"
#include "RE/REAugments.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <utility>

namespace SOS::Compat
{
	namespace
	{
		class WornArmorVisitor : public RE::IItemChangeVisitorAugment
		{
		public:
			VisitorReturn Visit(RE::InventoryEntryData* a_data) override
			{
				const auto* form = a_data ? a_data->object : nullptr;
				if (form && form->formType == RE::FormType::Armor) {
					if (auto* armor = skyrim_cast<RE::TESObjectARMO*>(const_cast<RE::TESBoundObject*>(form))) {
						armors.push_back(armor);
					}
				}
				return VisitorReturn::kContinue;
			}

			explicit WornArmorVisitor(std::vector<const RE::TESObjectARMO*>& a_out) :
				armors(a_out)
			{
				armors.clear();
				armors.reserve(32);
			}

			std::vector<const RE::TESObjectARMO*>& armors;
		};
	}

	std::uint32_t SlotMaskOf(const RE::TESObjectARMO* a_armor)
	{
		return a_armor ? a_armor->GetSlotMask().underlying() : 0u;
	}

	bool CollectWornArmors(RE::Actor* a_actor, std::vector<const RE::TESObjectARMO*>& a_out)
	{
		a_out.clear();
		if (!a_actor) {
			return false;
		}
		auto* changes = a_actor->GetInventoryChanges();
		if (!changes) {
			return false;
		}
		WornArmorVisitor visitor(a_out);
		RE::InventoryChangesAugments::ExecuteAugmentVisitorOnWorn(changes, &visitor);
		return true;
	}

	bool OverrideActive(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return false;
		}
		// Scripts and mods can ask while a save is still loading and the actor has
		// no 3D yet; answer with the vanilla data in that case.
		if (GameState::IsLoading() || !a_actor->Is3DLoaded()) {
			return false;
		}
		// The co-save owns the real switch (OutfitService::_enabled); the copy in
		// Settings is only the menu mirror and is not persisted.
		auto service = outfit_service_get_singleton_ptr();
		if (!service->inner().enabled_c()) {
			return false;
		}
		return service->inner().should_override(a_actor->GetFormID());
	}

	bool ComputeDisplaySet(RE::Actor* a_actor, std::vector<TESObjectARMOPtr>& a_out)
	{
		a_out.clear();
		if (!a_actor) {
			return false;
		}
		auto service = outfit_service_get_singleton_ptr();
		const auto* outfit = service->inner().current_outfit_ptr(a_actor->GetFormID());
		if (!outfit) {
			return false;
		}

		thread_local std::vector<const RE::TESObjectARMO*> worn;
		worn.clear();
		if (!CollectWornArmors(a_actor, worn)) {
			return false;
		}

		a_out = outfit->compute_display_set_c(rust::Slice<const RE::TESObjectARMO*>(worn.data(), worn.size()));
		return true;
	}

	bool DisplayHasKeyword(RE::Actor* a_actor, const RE::BGSKeyword* a_keyword)
	{
		if (!a_keyword) {
			return false;
		}
		std::vector<TESObjectARMOPtr> display;
		if (!ComputeDisplaySet(a_actor, display)) {
			return false;
		}
		for (const auto& item : display) {
			if (item.ptr && item.ptr->HasKeyword(a_keyword)) {
				return true;
			}
		}
		return false;
	}

	bool VanillaWornHasKeyword(RE::Actor* a_actor, const RE::BGSKeyword* a_keyword)
	{
		if (!a_actor || !a_keyword) {
			return false;
		}
		std::vector<const RE::TESObjectARMO*> worn;
		if (!CollectWornArmors(a_actor, worn)) {
			return false;
		}
		for (const auto* armor : worn) {
			if (armor && armor->HasKeyword(a_keyword)) {
				return true;
			}
		}
		return false;
	}

	void VanillaWornObjects(RE::Actor* a_actor, std::vector<RE::TESBoundObject*>& a_out)
	{
		a_out.clear();
		if (!a_actor) {
			return;
		}
		// Advanced Nudity Detection walks the inventory and collects every worn
		// object that is a keyword form; mirror that so a disabled compatibility
		// layer behaves exactly like the vanilla function.
		const auto inventory = a_actor->GetInventory();
		for (const auto& entry : inventory) {
			auto* item = entry.first;
			const auto& data = entry.second;
			if (!item || data.first <= 0 || !data.second) {
				continue;
			}
			if (!data.second->IsWorn()) {
				continue;
			}
			if (item->As<RE::BGSKeywordForm>()) {
				a_out.push_back(item);
			}
		}
	}
}
