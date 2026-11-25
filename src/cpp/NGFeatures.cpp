#include "NGFeatures.h"
#include "OutfitSystem.h"

#include <algorithm>
#include <fstream>
#include <random>
#include <sstream>

#include "I18n.h"
#include "NGHotkeys.h"
#include "MenuUI.h"
#include "MorphRefresh.h"
#include "bindings.h"
#include "core/OutfitService.h"
#include "core/Settings.h"
#include "RE/REAugments.h"
#include "Utility.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

namespace SOS::Features
{
	namespace
	{
		std::mt19937& Rng()
		{
			static thread_local std::mt19937 s_rng{ std::random_device{}() };
			return s_rng;
		}

		std::string CanonicalNA(const std::string& a_name)
		{
			std::string result(a_name);
			std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
				return static_cast<char>(std::tolower(c));
			});
			return result;
		}
	}

	void RefreshActor(RE::Actor* a_actor)
	{
		// Keep one refresh path for the whole plugin: refreshArmorFor also runs
		// the per-mod follow ups (Procedural Armor Damage diagnostics and the
		// Faster HDT-SMP reset), which the menu and the quick picker used to
		// miss when they only poked the AI process directly.
		::OutfitSystem::refreshArmorFor(a_actor);
	}

	std::vector<std::string> OutfitNames(bool a_favoritesOnly)
	{
		auto service = outfit_service_get_singleton_ptr();
		std::vector<std::string> result;
		for (const auto& name : service->inner().get_outfit_names(a_favoritesOnly)) {
			result.emplace_back(name);
		}
		return result;
	}

	std::vector<const RE::TESObjectARMO*> WornArmors(RE::Actor* a_actor)
	{
		std::vector<const RE::TESObjectARMO*> result;
		if (!a_actor) {
			return result;
		}
		auto* changes = a_actor->GetInventoryChanges();
		if (!changes) {
			return result;
		}

		class Visitor : public RE::IItemChangeVisitorAugment
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

			std::vector<const RE::TESObjectARMO*> armors;

			Visitor()
			{
				armors.reserve(32);
			}
		};

		Visitor visitor;
		RE::InventoryChangesAugments::ExecuteAugmentVisitorOnWorn(changes, &visitor);
		return visitor.armors;
	}

	std::vector<const RE::TESObjectARMO*> CarriedArmors(RE::Actor* a_actor)
	{
		std::vector<const RE::TESObjectARMO*> result;
		if (!a_actor) {
			return result;
		}
		auto* changes = a_actor->GetInventoryChanges();
		if (!changes) {
			return result;
		}

		class Visitor : public RE::IItemChangeVisitorAugment
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

			std::vector<const RE::TESObjectARMO*> armors;

			Visitor()
			{
				armors.reserve(32);
			}
		};

		Visitor visitor;
		RE::InventoryChangesAugments::ExecuteAugmentVisitor(changes, &visitor);
		return visitor.armors;
	}

	std::vector<RE::Actor*> ActorsNearPlayer()
	{
		std::vector<RE::Actor*> result;
		auto*                   player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return result;
		}
		auto* cell = player->GetParentCell();
		if (!cell) {
			return result;
		}
		for (const auto& ref : cell->GetRuntimeData().references) {
			auto* actor = skyrim_cast<RE::Actor*>(ref.get());
			if (!actor || actor->IsDead()) {
				continue;
			}
			if (!is_form_id_permitted(actor->GetFormID())) {
				continue;
			}
			result.push_back(actor);
		}
		return result;
	}

	// The old MCM toggled the quickslot spell through the SkyOutSysQuickslotManager
	// quest script. Doing it natively keeps the switch working even when the
	// Papyrus manager is not running.
	void ApplyQuickslotSpell()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}
		auto* spell = RE::TESForm::LookupByEditorID<RE::SpellItem>("SkyrimOutfitSystemQuickslotSpell");
		if (!spell) {
			// The record may have been renamed by a translation patch; fall back
			// to the fixed local form id of the shipped esp.
			if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
				spell = dataHandler->LookupForm<RE::SpellItem>(0x000182F, "SkyrimOutfitSystem.esp");
			}
		}
		if (!spell) {
			SKSE::log::warn("Quickslots: the SkyrimOutfitSystemQuickslotSpell form was not found; the switch has no effect.");
			return;
		}
		const bool enabled = Settings::Get().quickslotsEnabled;
		const bool known = player->HasSpell(spell);
		if (enabled && !known) {
			player->AddSpell(spell);
			SKSE::log::debug("Quickslots: the Change Outfit power was added to the player.");
		} else if (!enabled && known) {
			player->RemoveSpell(spell);
			SKSE::log::debug("Quickslots: the Change Outfit power was removed from the player.");
		}
	}

	bool CreateOutfitFromWorn(RE::Actor* a_actor, const char* a_name)
	{
		if (!a_actor || !a_name || !*a_name) {
			return false;
		}

		const auto worn = WornArmors(a_actor);
		if (worn.empty()) {
			SKSE::log::warn("CreateOutfitFromWorn: '{}' is wearing no armour.", a_actor->GetDisplayFullName());
			return false;
		}

		auto service = outfit_service_get_mut_singleton_ptr();
		if (!service->inner().modify_outfit(a_name, worn, {}, true)) {
			return false;
		}
		SKSE::log::debug("CreateOutfitFromWorn: '{}' stored as '{}' ({} pieces).", a_actor->GetDisplayFullName(), a_name, worn.size());
		return true;
	}

	std::string PickRandomOutfit(RE::Actor* a_actor, bool a_favoritesOnly)
	{
		if (!a_actor) {
			return {};
		}

		const auto& settings = Settings::Get();

		// Only hold the store lock while copying the candidate list. AssignOutfit
		// below takes the write lock and then refreshes the actor, which can call
		// back into the skinning hooks on the render thread.
		std::string              currentName;
		std::vector<std::string> candidates;
		{
			auto       service = outfit_service_get_singleton_ptr();
			const auto& inner = service->inner();

			const auto* current = inner.current_outfit_ptr(a_actor->GetFormID());
			if (current) {
				currentName = current->name_c();
			}

			// Fills the candidate list. When "favourites only" leaves nothing
			// (for example the single favourite is the outfit that is worn right
			// now) the caller falls back to the complete list, matching the
			// behaviour of the state based random outfits.
			const auto collect = [&](bool a_onlyFavourites, bool a_excludeCurrent) {
				const auto names = inner.get_outfit_names(a_onlyFavourites);
				for (const auto& name : names) {
					std::string candidate(name);
					if (a_excludeCurrent && !currentName.empty() &&
						_stricmp(candidate.c_str(), currentName.c_str()) == 0) {
						continue;
					}
					candidates.push_back(std::move(candidate));
				}
			};

			collect(a_favoritesOnly, settings.randomExcludeCurrent);
			if (candidates.empty() && a_favoritesOnly) {
				collect(false, settings.randomExcludeCurrent);
			}
			if (candidates.empty() && settings.randomExcludeCurrent) {
				// Only one outfit exists and it is the current one: rolling it
				// again is better than silently doing nothing.
				collect(false, false);
			}
		}

		if (candidates.empty()) {
			return {};
		}

		std::uniform_int_distribution<std::size_t> pick(0, candidates.size() - 1);
		const auto& chosen = candidates[pick(Rng())];

		if (!AssignOutfit(a_actor, chosen.c_str())) {
			return {};
		}
		return chosen;
	}

	bool ApplyCurrentStateOutfit(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return false;
		}
		{
			auto service = outfit_service_get_mut_singleton_ptr();
			OutfitSystem::setOutfitUsingState(service->inner(), a_actor);
		}
		RefreshActor(a_actor);
		return true;
	}

	bool AssignOutfit(RE::Actor* a_actor, const char* a_name)
	{
		if (!a_actor) {
			return false;
		}

		std::string previous;
		{
			auto service = outfit_service_get_mut_singleton_ptr();
			auto& inner = service->inner();
			if (const auto* current = inner.current_outfit_ptr(a_actor->GetFormID())) {
				previous = current->name_c().c_str();
			}
			if (a_name && *a_name) {
				if (!inner.has_outfit(a_name)) {
					return false;
				}
				inner.set_outfit_c(a_name, a_actor->GetFormID());
			} else {
				inner.set_outfit_c("", a_actor->GetFormID());
			}
		}

		// Re-wearing the outfit that is already assigned changes nothing in the
		// equip path, so the engine keeps the clones it built earlier and a
		// stale .tri deformation survives. Run the off/on cycle instead of a
		// plain refresh; both passes happen back to back, so the swap is as
		// quick as picking another outfit and never shows the real gear.
		if (!previous.empty() && a_name && previous == a_name &&
			MorphRefresh::RunIllusionCycle(a_actor, "the outfit was re-applied")) {
			return true;
		}

		// RefreshActor drives the AI equip path, which can call straight back
		// into the skinning hooks on the render thread. Never do that with the
		// outfit store lock held.
		RefreshActor(a_actor);
		return true;
	}

	std::string RandomOutfit(RE::BSScript::IVirtualMachine* a_registry, std::uint32_t a_stackId, RE::StaticFunctionTag*, RE::Actor* a_actor, bool a_favoritesOnly)
	{
		LogExit exitPrint("RandomOutfit"sv);
		if (SOS::Hotkeys::PowerActionJustHandled()) {
			// The key handler already rolled one when the power's key was pressed;
			// the script that runs after the cast animation must not roll again.
			SKSE::log::debug("RandomOutfit: skipped, the key press already rolled an outfit.");
			return {};
		}
		if (!a_actor) {
			if (a_registry) {
				a_registry->TraceStack("Cannot pick an outfit for a None RE::Actor.", a_stackId, RE::BSScript::IVirtualMachine::Severity::kError);
			}
			return {};
		}
		return PickRandomOutfit(a_actor, a_favoritesOnly);
	}

	std::int32_t GetOutfitCount(RE::BSScript::IVirtualMachine*, std::uint32_t, RE::StaticFunctionTag*)
	{
		auto service = outfit_service_get_singleton_ptr();
		return static_cast<std::int32_t>(service->inner().outfit_count());
	}

	std::string GetOutfitNameByIndex(RE::BSScript::IVirtualMachine*, std::uint32_t, RE::StaticFunctionTag*, std::int32_t a_index, bool a_favoritesOnly)
	{
		const auto names = OutfitNames(a_favoritesOnly);
		if (a_index < 0 || static_cast<std::size_t>(a_index) >= names.size()) {
			return {};
		}
		return names[static_cast<std::size_t>(a_index)];
	}

	std::vector<std::string> SearchOutfits(RE::BSScript::IVirtualMachine*, std::uint32_t, RE::StaticFunctionTag*, std::string a_query, bool a_favoritesOnly)
	{
		const auto needle = CanonicalNA(a_query);
		std::vector<std::string> result;
		for (const auto& name : OutfitNames(a_favoritesOnly)) {
			if (needle.empty() || CanonicalNA(name).find(needle) != std::string::npos) {
				result.push_back(name);
			}
		}
		return result;
	}

	// ---- OutfitSystemData.json (original mod format) -----------------------
	std::string OutfitDataJsonPath()
	{
		return *GetRuntimeDirectory() + "Data\\SKSE\\Plugins\\OutfitSystemData.json";
	}

	bool ExportOutfitsToJson(std::string& a_message)
	{
		const std::string path = OutfitDataJsonPath();
		std::string payload;
		{
			auto service = outfit_service_get_singleton_ptr();
			payload = std::string(service->inner().save_legacy_json_c());
		}

		std::ofstream file(path, std::ios::binary);
		if (!file) {
			a_message = I18n::T("Failed to open config for writing", "无法打开配置文件写入。");
			SKSE::log::error("Export: cannot open {} for writing.", path);
			return false;
		}
		file << payload;
		const bool good = file.good();
		file.close();
		if (!good) {
			a_message = I18n::T("Failed to write config", "写入配置文件失败。");
			SKSE::log::error("Export: writing {} failed.", path);
			return false;
		}
		a_message = std::string(I18n::T("Wrote JSON config to ", "已导出 JSON 配置到 ")) + path;
		SKSE::log::debug("Export: wrote {} bytes to {}.", payload.size(), path);
		return true;
	}

	bool ImportOutfitsFromJson(std::string& a_message)
	{
		const std::string path = OutfitDataJsonPath();
		std::ifstream file(path, std::ios::binary);
		if (!file) {
			a_message = I18n::T("Failed to open config for reading", "无法打开配置文件读取。");
			SKSE::log::error("Import: cannot open {} for reading.", path);
			return false;
		}
		std::stringstream buffer;
		buffer << file.rdbuf();
		if (buffer.bad()) {
			a_message = I18n::T("Failed to read config data", "读取配置文件失败。");
			SKSE::log::error("Import: reading {} failed.", path);
			return false;
		}
		const std::string json = buffer.str();

		bool ok = false;
		{
			auto service = outfit_service_get_mut_singleton_ptr();
			ok = service->inner().replace_with_legacy_json(rust::Str{ json.data(), json.size() }, SKSE::GetSerializationInterface());
		}
		if (!ok) {
			a_message = I18n::T("Failed to parse config data. Invalid syntax.", "无法解析配置数据，语法错误。");
			return false;
		}
		a_message = std::string(I18n::T("Read JSON config from ", "已从以下文件导入 JSON 配置：")) + path;
		SKSE::log::debug("Import: read {} bytes from {}.", json.size(), path);
		return true;
	}

	// ---- quick picker (Papyrus side) ---------------------------------------
	//
	// The "Change Outfit" lesser power calls OpenQuickPicker(). When the menu
	// framework is missing the call reports it instead of silently doing nothing,
	// which is what made the power look dead before.
	bool OpenQuickPicker(RE::BSScript::IVirtualMachine*, std::uint32_t, RE::StaticFunctionTag*)
	{
		if (SOS::Hotkeys::PowerActionJustHandled() && Menu::QuickPickerVisible()) {
			// Opened by the key press itself, before the power finished casting.
			return true;
		}
		if (!Menu::Register()) {
			RE::SendHUDMessage::ShowHUDMessage(I18n::T("Skyrim Outfit System: install SKSE Menu Framework to use the quick outfit picker.", "天际幻化系统：需要安装 SKSE Menu Framework 才能使用快速选择窗口。"), nullptr, false);
			SKSE::log::warn("Quick picker: the SKSE Menu Framework is not available.");
			return false;
		}
		if (!Menu::QuickPickerVisible()) {
			Menu::ShowQuickPicker(true);
		}
		return Menu::QuickPickerVisible();
	}

	void CloseQuickPicker(RE::BSScript::IVirtualMachine*, std::uint32_t, RE::StaticFunctionTag*)
	{
		Menu::ShowQuickPicker(false);
	}

	bool IsQuickPickerVisible(RE::BSScript::IVirtualMachine*, std::uint32_t, RE::StaticFunctionTag*)
	{
		return Menu::QuickPickerVisible();
	}

	bool IsQuickSlotRandom(RE::BSScript::IVirtualMachine*, std::uint32_t, RE::StaticFunctionTag*)
	{
		return Settings::Get().quickslotRandom;
	}

	void SetQuickSlotRandom(RE::BSScript::IVirtualMachine*, std::uint32_t, RE::StaticFunctionTag*, bool a_enabled)
	{
		Settings::Get().quickslotRandom = a_enabled;
		Settings::Get().Save();
		SKSE::log::debug("Quickslot spell: random outfit {}.", a_enabled ? "enabled" : "disabled");
	}
}
