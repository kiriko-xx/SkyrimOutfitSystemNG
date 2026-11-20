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
			std::unordered_map<std::string, std::vector<RE::TESObjectARMO*>> armors;   // lower name -> slot armours
			std::vector<RE::TESObjectARMO*>                                 all;      // every slot armour
		};

		std::mutex                   g_indexMutex;
		std::shared_ptr<const Index> g_index;

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
		if (!a_armor) {
			return false;
		}
		// Unnamed records are patch stubs or placeholders; a generated outfit
		// full of them is unusable because nothing in the menu identifies them.
		const char* editorId = a_armor->GetFormEditorID();
		if (!editorId || editorId[0] == '\0') {
			return false;
		}
		// The FX slot is a visual effect channel (magic shader, glow mesh); it is
		// never clothing. A record that only covers FX is dropped, one that
		// covers FX next to real slots stays.
		constexpr std::uint32_t kFxBit = 1u << 31;
		const auto mask = a_armor->GetSlotMask().underlying();
		if ((mask & ~kFxBit) == 0) {
			return false;
		}
		// No armour addon means no model, so the slot would clone an empty shape.
		if (a_armor->armorAddons.size() == 0) {
			return false;
		}
		for (const auto* addon : a_armor->armorAddons) {
			if (!addon) {
				continue;
			}
			const char* third = addon->bipedModels[RE::SEXES::kMale].GetModel();
			if (third && third[0] != '\0') {
				return true;
			}
			const char* first = addon->bipedModel1stPersons[RE::SEXES::kMale].GetModel();
			if (first && first[0] != '\0') {
				return true;
			}
		}
		return false;
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

	RE::TESObjectARMO* ResolveArmor(std::string_view a_plugin, std::uint32_t a_localId)
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
		auto* form = handler->LookupForm(a_localId, named->second);
		auto* armor = form ? form->As<RE::TESObjectARMO>() : nullptr;
		return IsSlotArmor(armor) ? armor : nullptr;
	}

	void Invalidate()
	{
		std::lock_guard lock(g_indexMutex);
		g_index.reset();
	}
}
