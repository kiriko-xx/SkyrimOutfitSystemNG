#include "core/EspOutfits.h"

#include "core/EspOutfitMatch.h"
#include "core/EspOutfitNames.h"
#include "core/OutfitService.h"
#include "core/PluginScan.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <algorithm>
#include <set>
#include <tuple>

namespace SOS::EspOutfits
{
	static constexpr auto kPolicy = "XXXO";  // require equipped

	std::vector<PluginSummary> PluginsWithArmor()
	{
		std::vector<PluginSummary> out;
		for (const auto& entry : PluginScan::PluginsWithArmor()) {
			// The list shows every plugin that carries slot armour; a plugin whose
			// pieces all fail the clothing test is still worth showing, because that
			// is how the user finds out what the plugin holds. 1.5.39 dropped every
			// plugin with outfitArmors == 0, which emptied the list completely once
			// the clothing test was too strict.
			if (entry.armors == 0) {
				continue;
			}
			PluginSummary summary;
			summary.name = entry.name;
			summary.order = entry.order;
			summary.armors = entry.armors;
			summary.outfitArmors = entry.outfitArmors;
			out.push_back(std::move(summary));
		}
		return out;
	}

	std::string BaseNameFor(const std::string& a_plugin)
	{
		std::string base = a_plugin;
		const auto  dot = base.find_last_of('.');
		if (dot != std::string::npos && dot > 0) {
			base.erase(dot);
		}
		return base;
	}

	std::string SlotNumbers(std::uint32_t a_mask)
	{
		return Names::SlotNumbers(a_mask);
	}

	Preview PreviewFromPlugin(const std::string& a_plugin)
	{
		Preview preview;
		preview.plugin = a_plugin;
		for (const auto* armor : PluginScan::ArmorsOf(a_plugin)) {
			const bool wearable = PluginScan::IsOutfitArmor(armor);
			if (!armor) {
				++preview.notClothing;
				continue;
			}
			const char* full = armor->GetFullName();
			const char* editor = armor->GetFormEditorID();
			const auto mask = armor->GetSlotMask().underlying();
			const bool ordinary = wearable;  // keep the original mask, including FX on a mixed-slot piece
			SKSE::log::info("ESP outfits: input [{}] id={:08X} mask={:08X} editor='{}' name='{}' slots={} {}",
				a_plugin, armor->GetFormID(), armor->GetSlotMask().underlying(), editor ? editor : "", full ? full : "",
				Names::SlotNumbers(mask), ordinary ? "wearable" : "not clothing");
			if (!ordinary) {
				++preview.notClothing;
				continue;
			}
			preview.pieces.push_back({ full ? full : "", editor ? editor : "", armor->GetFormID(), mask });
		}
		std::sort(preview.pieces.begin(), preview.pieces.end(), [](const auto& lhs, const auto& rhs) {
			return std::tie(lhs.editorId, lhs.formId) < std::tie(rhs.editorId, rhs.formId);
		});
		std::vector<Match::Piece> input;
		input.reserve(preview.pieces.size());
		for (const auto& piece : preview.pieces) {
			// FULL comes from the game, including the active localization. Do not
			// compare an EditorID with translated FULL names or guess a code page.
			auto key = Names::ComparisonKey(piece.name);
			if (preview.pieces.size() == 1 && piece.name.empty()) {
				key = Names::ComparisonKey(piece.editorId);  // no cross-record matching needed
			}
			input.push_back({ std::move(key), piece.mask });
		}
		const auto groups = Match::Plan(input);
		const auto base = BaseNameFor(a_plugin);
		const auto maxName = static_cast<std::size_t>(outfit_service_get_singleton_ptr()->inner().max_outfit_name_len());
		std::vector<bool> included(preview.pieces.size(), false);
		std::set<Match::Key> usedNames;
		for (const auto& group : groups) {
			const auto label = Names::OriginalPrefix(preview.pieces[group.members.front()].name, group.prefix);
			if (group.issue) {
				SKSE::log::warn("ESP outfits: pending [{}] family='{}' pieces={} reason={}", a_plugin, label, group.members.size(), group.issue);
				continue;
			}
			if (group.variants.size() > Match::kMaxOutfits - preview.outfits.size()) {
				preview.truncated = true;
				continue;
			}
			for (std::size_t i = 0; i < group.variants.size(); ++i) {
				auto name = Names::PresetName(base, label, groups.size(), group.variants.size(), i);
				if (usedNames.contains(Names::ComparisonKey(name))) {
					SKSE::log::warn("ESP outfits: pending [{}] family='{}' reason=duplicate-preset-name", a_plugin, label);
					continue;
				}
				if (name.empty() || name.size() > maxName || Names::ComparisonKey(name).empty()) {
					SKSE::log::warn("ESP outfits: pending [{}] family='{}' reason=unsupported-preset-name", a_plugin, label);
					continue;
				}
				usedNames.insert(Names::ComparisonKey(name));
				const auto& variant = group.variants[i];
				for (const auto index : variant) {
					included[index] = true;
					const auto& piece = preview.pieces[index];
					SKSE::log::info("ESP outfits: candidate [{}] preset='{}' id={:08X} slots={} name='{}'",
						a_plugin, name, piece.formId, Names::SlotNumbers(piece.mask), piece.name);
				}
				preview.outfits.push_back({ std::move(name), variant });
			}
		}
		for (std::size_t i = 0; i < included.size(); ++i) {
			if (!included[i]) {
				preview.unresolved.push_back(i);
				SKSE::log::warn("ESP outfits: unplaced [{}] id={:08X}", a_plugin, preview.pieces[i].formId);
			}
		}
		{
			auto guard = outfit_service_get_singleton_ptr();
			for (auto& preset : preview.outfits) {
				preset.existing = guard->inner().has_outfit(preset.name.c_str());
			}
		}
		SKSE::log::info("ESP outfits: preview [{}] candidates={} pending={} filtered={} capped={}", a_plugin,
			preview.outfits.size(), preview.unresolved.size(), preview.notClothing, preview.truncated);
		return preview;
	}

	Result ApplyPreview(const Preview& a_preview, bool a_favorite)
	{
		Result result;
		auto guard = outfit_service_get_mut_singleton_ptr();
		auto& service = guard->inner();
		for (const auto& outfit : a_preview.outfits) {
			if (service.has_outfit(outfit.name.c_str())) {
				++result.skipped;
				SKSE::log::warn("ESP outfits: skipped existing preset [{}] '{}'", a_preview.plugin, outfit.name);
				continue;
			}
			std::vector<const RE::TESObjectARMO*> chosen;
			std::uint32_t occupied = 0;
			for (const auto index : outfit.pieces) {
				const auto& item = a_preview.pieces.at(index);
				const auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(item.formId);
				if (!PluginScan::IsOutfitArmor(armor) || armor->GetSlotMask().underlying() != item.mask ||
					(occupied & item.mask) != 0) {
					break;
				}
				occupied |= item.mask;
				chosen.push_back(armor);
			}
			if (chosen.empty() || chosen.size() != outfit.pieces.size() ||
				!service.create_outfit_preset(outfit.name, chosen, kPolicy, {}, a_favorite)) {
				++result.skipped;
				SKSE::log::warn("ESP outfits: skipped invalid preset [{}] '{}'", a_preview.plugin, outfit.name);
				continue;
			}
			result.names.push_back(outfit.name);
		}
		SKSE::log::info("ESP outfits: confirmed [{}] created={} skipped={}", a_preview.plugin, result.names.size(), result.skipped);
		return result;
	}
}
