#include "core/PluginScan.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

namespace SOS::PluginScan
{
	namespace
	{
		std::string LowerCopy(std::string_view a_text)
		{
			std::string out;
			out.reserve(a_text.size());
			for (const char c : a_text) {
				out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
			}
			return out;
		}

		struct Index
		{
			std::vector<PluginEntry>                                        plugins;
			std::unordered_map<std::string, std::string>                    display;  // lower name -> stored name
			std::unordered_map<std::string, bool>                           light;    // lower name -> light file
			std::unordered_map<std::string, std::vector<RE::TESObjectARMO*>> armors;   // lower name -> slot armours
			std::vector<RE::TESObjectARMO*>                                 all;      // every slot armour
		};

		std::mutex                   g_indexMutex;
		std::shared_ptr<const Index> g_index;

		enum class OutfitVerdict
		{
			kWearable,
			kUnnamed,
			kFxOnly,
			kNoAddon,
			kNoModel,
		};

		// Why a slot armour record does or does not count as ordinary clothing.
		// Split out from IsOutfitArmor so the scan can report the reasons instead
		// of leaving an empty list unexplained.
		OutfitVerdict Verdict(const RE::TESObjectARMO* a_armor)
		{
			if (!a_armor) {
				return OutfitVerdict::kUnnamed;
			}
			// A record the menu cannot name is unusable: a generated outfit built
			// from "unnamed record" tells the user nothing. Either the editor id
			// or the display name is enough; 1.5.39 only accepted the editor id.
			const char* editorId = a_armor->GetFormEditorID();
			const char* fullName = a_armor->GetFullName();
			const bool  named = (editorId && editorId[0] != '\0') || (fullName && fullName[0] != '\0');
			if (!named) {
				return OutfitVerdict::kUnnamed;
			}
			constexpr std::uint32_t kFxBit = 1u << 31;
			const auto              mask = a_armor->GetSlotMask().underlying();
			if ((mask & ~kFxBit) == 0) {
				return OutfitVerdict::kFxOnly;
			}
			if (a_armor->armorAddons.size() == 0) {
				return OutfitVerdict::kNoAddon;
			}
			// Female only clothing packs are the norm, so both sexes count. 1.5.39
			// only looked at the male model, judged the whole load order as "not
			// clothing" and left the plugin picker empty.
			for (const auto* addon : a_armor->armorAddons) {
				if (!addon) {
					continue;
				}
				for (const auto sex : { RE::SEXES::kMale, RE::SEXES::kFemale }) {
					const char* third = addon->bipedModels[sex].GetModel();
					if (third && third[0] != '\0') {
						return OutfitVerdict::kWearable;
					}
					const char* first = addon->bipedModel1stPersons[sex].GetModel();
					if (first && first[0] != '\0') {
						return OutfitVerdict::kWearable;
					}
				}
			}
			return OutfitVerdict::kNoModel;
		}

		void CollectPlugins(Index& a_index, RE::TESDataHandler& a_handler)
		{
			std::uint32_t order = 0;
			if (const auto* const* mods = a_handler.GetLoadedMods()) {
				const auto count = a_handler.GetLoadedModCount();
				for (std::uint8_t i = 0; i < count; ++i) {
					const auto* file = mods[i];
					if (!file) {
						continue;
					}
					const std::string_view name = file->GetFilename();
					if (name.empty()) {
						continue;
					}
					PluginEntry entry;
					entry.name = std::string{ name };
					entry.order = order++;
					entry.light = false;
					a_index.display.emplace(LowerCopy(entry.name), entry.name);
					a_index.light.emplace(LowerCopy(entry.name), entry.light);
					a_index.plugins.push_back(std::move(entry));
				}
			}
			if (auto** lightMods = a_handler.GetLoadedLightMods()) {
				const auto count = a_handler.GetLoadedLightModCount();
				for (std::uint16_t i = 0; i < count; ++i) {
					const auto* file = lightMods[i];
					if (!file) {
						continue;
					}
					const std::string_view name = file->GetFilename();
					if (name.empty()) {
						continue;
					}
					PluginEntry entry;
					entry.name = std::string{ name };
					entry.order = order++;
					entry.light = true;
					a_index.display.emplace(LowerCopy(entry.name), entry.name);
					a_index.light.emplace(LowerCopy(entry.name), entry.light);
					a_index.plugins.push_back(std::move(entry));
				}
			}
		}

		void CollectArmors(Index& a_index, RE::TESDataHandler& a_handler)
		{
			auto& forms = a_handler.GetFormArray(RE::FormType::Armor);
			a_index.all.reserve(forms.size());
			for (auto* form : forms) {
				auto* armor = form ? form->As<RE::TESObjectARMO>() : nullptr;
				if (!IsSlotArmor(armor)) {
					continue;
				}
				const std::string owner = OwningPlugin(armor);
				if (owner.empty()) {
					continue;
				}
				a_index.all.push_back(armor);
				a_index.armors[LowerCopy(owner)].push_back(armor);
			}
			SKSE::log::debug("PluginScan: {} plugins loaded, {} slot armour record(s) indexed.", a_index.plugins.size(), a_index.all.size());
			std::size_t wearable = 0;
			std::size_t unnamed = 0;
			std::size_t fxOnly = 0;
			std::size_t noAddon = 0;
			std::size_t noModel = 0;
			std::size_t withArmor = 0;
			for (const auto& [key, list] : a_index.armors) {
				if (!list.empty()) {
					++withArmor;
				}
			}
			for (const auto* armor : a_index.all) {
				switch (Verdict(armor)) {
				case OutfitVerdict::kWearable: ++wearable; break;
				case OutfitVerdict::kUnnamed: ++unnamed; break;
				case OutfitVerdict::kFxOnly: ++fxOnly; break;
				case OutfitVerdict::kNoAddon: ++noAddon; break;
				case OutfitVerdict::kNoModel: ++noModel; break;
				}
			}
			SKSE::log::info("PluginScan: {} of {} slot armour record(s) count as clothing ({} unnamed, {} FX only, {} without an addon, {} without a model); {} plugin(s) carry slot armour.", wearable, a_index.all.size(), unnamed, fxOnly, noAddon, noModel, withArmor);
		}

		std::shared_ptr<const Index> EnsureIndex()
		{
			std::lock_guard lock(g_indexMutex);
			if (g_index) {
				return g_index;
			}
			auto index = std::make_shared<Index>();
			if (auto* handler = RE::TESDataHandler::GetSingleton()) {
				CollectPlugins(*index, *handler);
				CollectArmors(*index, *handler);
			} else {
				SKSE::log::warn("PluginScan: no data handler; the load order is empty.");
			}
			g_index = index;
			return g_index;
		}
	}// namespace

	std::vector<PluginEntry> LoadedPlugins()
	{
		const auto index = EnsureIndex();
		return index->plugins;
	}

	std::vector<PluginSummary> PluginsWithArmor()
	{
		const auto index = EnsureIndex();
		std::vector<PluginSummary> out;
		out.reserve(index->plugins.size());
		for (const auto& entry : index->plugins) {
			const auto it = index->armors.find(LowerCopy(entry.name));
			if (it == index->armors.end() || it->second.empty()) {
				continue;
			}
			PluginSummary summary;
			summary.name = entry.name;
			summary.order = entry.order;
			summary.armors = it->second.size();
			for (const auto* armor : it->second) {
				if (IsOutfitArmor(armor)) {
					++summary.outfitArmors;
				}
			}
			out.push_back(std::move(summary));
		}
		return out;
	}

	std::string OwningPlugin(const RE::TESForm* a_form)
	{
		if (!a_form) {
			return {};
		}
		const auto* file = a_form->GetFile(0);
		if (!file) {
			return {};
		}
		const std::string_view name = file->GetFilename();
		return name.empty() ? std::string{} : std::string{ name };
	}

	bool IsSlotArmor(const RE::TESObjectARMO* a_armor)
	{
		return a_armor && a_armor->GetSlotMask().underlying() != 0;
	}

	bool IsOutfitArmor(const RE::TESObjectARMO* a_armor)
	{
		return Verdict(a_armor) == OutfitVerdict::kWearable;
	}

	std::vector<RE::TESObjectARMO*> ArmorsOf(std::string_view a_plugin)
	{
		if (a_plugin.empty()) {
			return {};
		}
		const auto index = EnsureIndex();
		const auto it = index->armors.find(LowerCopy(a_plugin));
		if (it == index->armors.end()) {
			return {};
		}
		return it->second;
	}

	bool IsLightPlugin(std::string_view a_plugin)
	{
		if (a_plugin.empty()) {
			return false;
		}
		const auto index = EnsureIndex();
		const auto it = index->light.find(LowerCopy(a_plugin));
		return it != index->light.end() && it->second;
	}

	std::uint32_t LocalIdFor(std::string_view a_plugin, std::uint32_t a_formId)
	{
		// A light file addresses its records with 12 bits; the light index lives
		// above them in the runtime id and must never be stored with them.
		// 1.5.39-1.5.42 stored formId & 0xFFFFFF for every plugin, which for a
		// light file kept the light index (ids like 002E280C). LookupForm then
		// adds the light index again, no form is found, and every golden item of
		// that plugin showed up as "<not in this load order>".
		return IsLightPlugin(a_plugin) ? (a_formId & 0x00000FFFu) : (a_formId & 0x00FFFFFFu);
	}

	RE::TESForm* ResolveForm(std::string_view a_plugin, std::uint32_t a_localId)
	{
		if (a_plugin.empty()) {
			return nullptr;
		}
		auto* handler = RE::TESDataHandler::GetSingleton();
		if (!handler) {
			return nullptr;
		}
		const auto index = EnsureIndex();
		const auto named = index->display.find(LowerCopy(a_plugin));
		if (named == index->display.end()) {
			return nullptr;
		}
		// First the width the plugin actually uses, then the raw 24 bit value:
		// entries written by 1.5.39-1.5.42 (and ids typed in full by the user)
		// are repaired here instead of being reported as missing.
		const std::uint32_t compact = LocalIdFor(named->second, a_localId);
		if (auto* form = handler->LookupForm(compact, named->second)) {
			return form;
		}
		const std::uint32_t wide = a_localId & 0x00FFFFFFu;
		if (compact != wide) {
			return handler->LookupForm(wide, named->second);
		}
		return nullptr;
	}

	RE::TESObjectARMO* ResolveArmor(std::string_view a_plugin, std::uint32_t a_localId)
	{
		auto* form = ResolveForm(a_plugin, a_localId);
		auto* armor = form ? form->As<RE::TESObjectARMO>() : nullptr;
		return IsSlotArmor(armor) ? armor : nullptr;
	}

	void Invalidate()
	{
		std::lock_guard lock(g_indexMutex);
		g_index.reset();
	}
}
