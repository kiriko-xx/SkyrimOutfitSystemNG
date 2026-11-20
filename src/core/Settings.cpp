#include "core/Settings.h"

#include "core/GoldenItems.h"

#include <algorithm>

#include <SimpleIni.h>

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <filesystem>
#include <vector>

namespace SOS
{
	namespace
	{
		// The packaged ini lives in Data/SKSE/Plugins (next to the DLL), which is
		// where users expect to edit it. A copy in the SKSE log directory is also
		// accepted so that a user can override the packaged defaults without
		// touching their mod manager's files.
		std::vector<std::filesystem::path> SettingsPaths()
		{
			std::vector<std::filesystem::path> paths;
			std::error_code ec;
			auto runtime = std::filesystem::current_path(ec);
			if (!ec) {
				paths.push_back(runtime / "Data" / "SKSE" / "Plugins" / "SkyrimOutfitSystemSE.ini");
			}
			if (auto dir = SKSE::log::log_directory()) {
				paths.push_back(*dir / "SkyrimOutfitSystemSE.ini");
			}
			return paths;
		}
	}

	Settings& Settings::Get()
	{
		static Settings s_settings;
		return s_settings;
	}

	void Settings::Load()
	{
		// Built locally and published under the golden lock: ResolveGoldenItems()
		// runs with that lock held, so a reload must never reallocate the vector
		// the render thread is walking.
		std::vector<std::pair<std::string, RE::FormID>> golden;
		const auto publishGolden = [this, &golden]() {
			std::lock_guard lock(_goldenMutex);
			goldenItems = golden;
			std::atomic_store(&_goldenSnapshot,
				std::make_shared<const std::vector<std::pair<std::string, RE::FormID>>>(golden));
		};

		auto candidates = SettingsPaths();
		if (candidates.empty()) {
			SKSE::log::warn("Settings: could not resolve any ini path; using defaults.");
			publishGolden();
			InvalidateResolvedForms();
			return;
		}

		CSimpleIniA ini;
		ini.SetUnicode();
		std::filesystem::path path;
		bool loaded = false;
		for (const auto& candidate : candidates) {
			std::error_code ec;
			if (!std::filesystem::exists(candidate, ec) || ec) {
				continue;
			}
			if (ini.LoadFile(candidate.string().c_str()) >= 0) {
				path = candidate;
				loaded = true;
				break;
			}
			SKSE::log::warn("Settings: failed to parse {}; trying the next location.", candidate.string());
		}
		if (!loaded) {
			SKSE::log::debug("Settings: no readable ini (looked at {} locations); using defaults.", candidates.size());
			InvalidateResolvedForms();
			return;
		}

		quickslotsEnabled = ini.GetBoolValue("General", "QuickslotsEnabled", quickslotsEnabled);
		extraLogging = ini.GetBoolValue("Debug", "ExtraLogging", extraLogging);
		installHooks = ini.GetBoolValue("Debug", "InstallHooks", installHooks);

		compatAdvancedNudityDetection = ini.GetBoolValue("Compatibility", "AdvancedNudityDetection", compatAdvancedNudityDetection);
		compatProceduralArmorDamage = ini.GetBoolValue("Compatibility", "ProceduralArmorDamage", compatProceduralArmorDamage);
		autoResetSmp = ini.GetBoolValue("Compatibility", "AutoResetSMP", autoResetSmp);
		sceneOutfitSwitching = ini.GetBoolValue("Compatibility", "SceneOutfitSwitching", sceneOutfitSwitching);
		reillusionGapMs = static_cast<int>(ini.GetLongValue("Compatibility", "ReillusionGapMs", reillusionGapMs));
		if (reillusionGapMs < 0) {
			reillusionGapMs = 0;
		} else if (reillusionGapMs > 1000) {
			reillusionGapMs = 1000;
		}
		morphProviderFirst = ini.GetBoolValue("Compatibility", "MorphProviderFirst", morphProviderFirst);

		randomFavoritesOnly = ini.GetBoolValue("RandomOutfit", "FavoritesOnly", randomFavoritesOnly);
		randomExcludeCurrent = ini.GetBoolValue("RandomOutfit", "ExcludeCurrent", randomExcludeCurrent);
		quickslotRandom = ini.GetBoolValue("RandomOutfit", "QuickSlotRandom", quickslotRandom);
		if (const char* hotkey = ini.GetValue("RandomOutfit", "Hotkey", randomOutfitHotkey.c_str())) {
			randomOutfitHotkey = hotkey;
		}
		if (const char* hotkey = ini.GetValue("RandomOutfit", "QuickPickerHotkey", quickPickerHotkey.c_str())) {
			quickPickerHotkey = hotkey;
		}

		if (const char* value = ini.GetValue("UI", "Language", this->language.c_str())) {
			this->language = value;
		}
		outfitPageSize = static_cast<int>(ini.GetLongValue("UI", "OutfitPageSize", outfitPageSize));
		quickPickerPageSize = static_cast<int>(ini.GetLongValue("UI", "QuickPickerPageSize", quickPickerPageSize));
		outfitListWidth = static_cast<int>(ini.GetLongValue("UI", "OutfitListWidth", outfitListWidth));
		outfitPageSize = std::clamp(outfitPageSize, 5, 500);
		quickPickerPageSize = std::clamp(quickPickerPageSize, 5, 500);
		outfitListWidth = std::clamp(outfitListWidth, 240, 1200);
		quickPickerX = static_cast<int>(ini.GetLongValue("UI", "QuickPickerX", quickPickerX));
		quickPickerY = static_cast<int>(ini.GetLongValue("UI", "QuickPickerY", quickPickerY));
		quickPickerWidth = static_cast<int>(ini.GetLongValue("UI", "QuickPickerWidth", quickPickerWidth));
		quickPickerHeight = static_cast<int>(ini.GetLongValue("UI", "QuickPickerHeight", quickPickerHeight));
		quickPickerWidth = std::clamp(quickPickerWidth, 320, 4096);
		quickPickerHeight = std::clamp(quickPickerHeight, 240, 4096);

		espOutfitAutoFavorite = ini.GetBoolValue("Customization", "EspOutfitAutoFavorite", espOutfitAutoFavorite);
		setDefaultSlotPolicies = ini.GetBoolValue("Customization", "NewOutfitSetDefaultSlotPolicies", setDefaultSlotPolicies);
		if (const char* base = ini.GetValue("Customization", "NewOutfitDefaultBasePolicy", defaultBasePolicy.c_str())) {
			defaultBasePolicy = base;
		}

		// The golden item store owns the list now; the ini key is only the legacy
		// source and is read exactly once, then written back as migrated.
		goldenIniMigrated = ini.GetBoolValue("Compatibility", "GoldenItemsMigrated", false);
		if (!goldenIniMigrated) {
			golden.emplace_back("Dragonborn.esm", 0x00021739);  // Dragon Aspect Armor
		}

		if (!goldenIniMigrated) if (const char* goldenValue = ini.GetValue("Compatibility", "GoldenItems", nullptr)) {
			std::string list(goldenValue);
			std::size_t start = 0;
			while (start <= list.size()) {
				auto end = list.find(',', start);
				auto token = list.substr(start, end == std::string::npos ? std::string::npos : end - start);
				auto colon = token.find(':');
				if (colon != std::string::npos) {
					auto plugin = token.substr(0, colon);
					auto id = std::strtoul(token.c_str() + colon + 1, nullptr, 16);
					if (!plugin.empty() && id != 0) {
						golden.emplace_back(plugin, static_cast<RE::FormID>(id));
					}
				}
				if (end == std::string::npos) {
					break;
				}
				start = end + 1;
			}
		}

		publishGolden();
		SKSE::log::debug("Settings: loaded {} (extra logging: {}, base policy: {}, golden items: {}).",
			path.string(), extraLogging, defaultBasePolicy, golden.size());
		InvalidateResolvedForms();
	}

	void Settings::Save()
	{
		auto candidates = SettingsPaths();
		std::filesystem::path target;
		std::error_code ec;
		for (const auto& candidate : candidates) {
			if (std::filesystem::exists(candidate, ec) && !ec) {
				target = candidate;
				break;
			}
		}
		if (target.empty()) {
			if (candidates.empty()) {
				SKSE::log::warn("Settings: no ini path to write to.");
				return;
			}
			target = candidates.front();
		}

		// Re-read first so that the comments of the packaged file survive.
		CSimpleIniA ini;
		ini.SetUnicode();
		ini.LoadFile(target.string().c_str());
		ini.SetBoolValue("General", "QuickslotsEnabled", quickslotsEnabled);
		ini.SetBoolValue("Debug", "ExtraLogging", extraLogging);
		ini.SetBoolValue("Debug", "InstallHooks", installHooks);
		ini.SetBoolValue("Compatibility", "AdvancedNudityDetection", compatAdvancedNudityDetection);
		ini.SetBoolValue("Compatibility", "ProceduralArmorDamage", compatProceduralArmorDamage);
		ini.SetBoolValue("Compatibility", "AutoResetSMP", autoResetSmp);
		ini.SetBoolValue("Compatibility", "SceneOutfitSwitching", sceneOutfitSwitching);
		ini.SetLongValue("Compatibility", "ReillusionGapMs", reillusionGapMs);
		ini.SetBoolValue("Compatibility", "MorphProviderFirst", morphProviderFirst);
		ini.SetValue("RandomOutfit", "Hotkey", randomOutfitHotkey.c_str());
		ini.SetValue("RandomOutfit", "QuickPickerHotkey", quickPickerHotkey.c_str());
		ini.SetBoolValue("RandomOutfit", "FavoritesOnly", randomFavoritesOnly);
		ini.SetBoolValue("RandomOutfit", "ExcludeCurrent", randomExcludeCurrent);
		ini.SetBoolValue("RandomOutfit", "QuickSlotRandom", quickslotRandom);
		ini.SetValue("UI", "Language", language.c_str());
		ini.SetLongValue("UI", "OutfitPageSize", outfitPageSize);
		ini.SetLongValue("UI", "QuickPickerPageSize", quickPickerPageSize);
		ini.SetLongValue("UI", "OutfitListWidth", outfitListWidth);
		ini.SetLongValue("UI", "QuickPickerX", quickPickerX);
		ini.SetLongValue("UI", "QuickPickerY", quickPickerY);
		ini.SetLongValue("UI", "QuickPickerWidth", quickPickerWidth);
		ini.SetLongValue("UI", "QuickPickerHeight", quickPickerHeight);
		ini.SetBoolValue("Customization", "EspOutfitAutoFavorite", espOutfitAutoFavorite);
		ini.SetBoolValue("Compatibility", "GoldenItemsMigrated", goldenIniMigrated);
		ini.SetBoolValue("Customization", "NewOutfitSetDefaultSlotPolicies", setDefaultSlotPolicies);
		ini.SetValue("Customization", "NewOutfitDefaultBasePolicy", defaultBasePolicy.c_str());
		if (ini.SaveFile(target.string().c_str()) < 0) {
			SKSE::log::warn("Settings: could not write {} (read-only?).", target.string());
		} else {
			SKSE::log::debug("Settings: saved to {}.", target.string());
		}
	}

	void Settings::InvalidateResolvedForms()
	{
		std::lock_guard lock(_goldenMutex);
		_goldenResolved.store(false, std::memory_order_release);
		std::atomic_store(&_resolvedGolden, std::shared_ptr<const std::unordered_set<RE::FormID>>{});
	}

	void Settings::ResolveGoldenItems() const
	{
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			return;
		}

		// Build the whole set locally first: readers must never observe a
		// half-filled snapshot while the menu reloads the configuration.
		// _goldenMutex is held by WarmUp() when it calls this, so read the
		// immutable snapshot instead of taking the lock again.
		const auto items = std::atomic_load(&_goldenSnapshot);
		if (!items) {
			return;
		}
		auto resolved = std::make_shared<std::unordered_set<RE::FormID>>();
		resolved->reserve(items->size());
		for (const auto& [plugin, localId] : *items) {
			auto* form = dataHandler->LookupForm(localId, plugin);
			if (form) {
				resolved->insert(form->GetFormID());
			} else {
				SKSE::log::warn("Settings: golden item {}:{:08X} was not found in this load order.", plugin, localId);
			}
		}
		std::atomic_store(&_resolvedGolden, std::shared_ptr<const std::unordered_set<RE::FormID>>{ std::move(resolved) });
		_goldenResolved.store(true, std::memory_order_release);
	}

	bool Settings::IsGoldenItem(const RE::TESObjectARMO* a_armor) const
	{
		// The set is owned by SOS::Golden (it is grouped by plugin and the menu
		// edits it). This entry point stays because the skinning hooks already ask
		// here.
		return Golden::IsGolden(a_armor);
	}
}// namespace SOS
