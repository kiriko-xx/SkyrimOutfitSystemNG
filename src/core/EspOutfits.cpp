#include "core/EspOutfits.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <set>

#include "core/OutfitService.h"
#include "core/PluginScan.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

namespace SOS::EspOutfits
{
	namespace
	{
		// "Require equipped": the generated outfit only shows on a slot the
		// actor wears something on.
		constexpr const char* kPolicy = "XXXO";
		// A plugin with dozens of alternatives would otherwise create dozens of
		// outfits per click; the rest is reported instead of silently dropped.
		constexpr std::size_t kMaxVariants = 32;
		constexpr std::uint32_t kSlotCount = 32;
		// Index 31 of the display mask is the FX channel (creation kit slot 61).
		// It carries effects, not clothing, so it is never part of a generated
		// outfit even when the record also covers real slots.
		constexpr std::uint32_t kFxBit = 1u << 31;
	}// namespace

	std::vector<PluginSummary> PluginsWithArmor()
	{
		std::vector<PluginSummary> out;
		for (const auto& entry : PluginScan::PluginsWithArmor()) {
			if (entry.outfitArmors == 0) {
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
		const auto dot = base.find_last_of('.');
		if (dot != std::string::npos && dot > 0) {
			base.erase(dot);
		}
		return base;
	}

	std::string OutfitNameFor(const std::string& a_base, std::size_t a_index)
	{
		if (a_index == 0) {
			return a_base;
		}
		return a_base + "_" + std::to_string(a_index);
	}

	Result GenerateFromPlugin(const std::string& a_plugin, bool a_favorite)
	{
		Result result;
		if (a_plugin.empty()) {
			result.message = "No plugin was selected.";
			return result;
		}

		const auto armors = PluginScan::ArmorsOf(a_plugin);
		if (armors.empty()) {
			result.message = "That plugin holds no armour that covers a display slot.";
			return result;
		}

		// Only ordinary clothing becomes an outfit: an FX effect, a record
		// without an editor id and a record without a model are all skipped, and
		// the count is reported so the list can show how many pieces a plugin
		// really offers. Golden items do not do this: they take any record.
		std::vector<const RE::TESObjectARMO*> wearables;
		wearables.reserve(armors.size());
		std::size_t notClothing = 0;
		for (const auto* armor : armors) {
			if (PluginScan::IsOutfitArmor(armor)) {
				wearables.push_back(armor);
			} else {
				++notClothing;
			}
		}
		if (wearables.empty()) {
			result.message = "That plugin holds no wearable clothing: every slot record is an FX effect, has no editor id or has no model.";
			SKSE::log::info("ESP outfits: {} ({} slot record(s), all of them not clothing).", result.message, armors.size());
			return result;
		}

		// One bucket per display slot, in slot order. A piece that covers two
		// slots (a full body suit, for example) is listed in both, which is what
		// "same slot" means to the user.
		std::array<std::vector<const RE::TESObjectARMO*>, kSlotCount> bySlot;
		for (const auto* armor : wearables) {
			if (!armor) {
				continue;
			}
			const auto mask = armor->GetSlotMask().underlying() & ~kFxBit;
			for (std::uint32_t bit = 0; bit < kSlotCount; ++bit) {
				if ((mask & (1u << bit)) != 0) {
					bySlot[bit].push_back(armor);
				}
			}
		}

		std::size_t variants = 0;
		for (const auto& list : bySlot) {
			variants = std::max(variants, list.size());
		}
		if (variants == 0) {
			result.message = "That plugin holds no wearable clothing that covers a display slot.";
			return result;
		}
		const bool truncated = variants > kMaxVariants;
		if (truncated) {
			variants = kMaxVariants;
		}

		const std::string base = BaseNameFor(a_plugin);
		std::size_t skipped = 0;
		{
			auto guard = outfit_service_get_mut_singleton_ptr();
			auto& service = guard->inner();
			for (std::size_t index = 0; index < variants; ++index) {
				const std::string name = OutfitNameFor(base, index);
				// Slot order decides which alternative goes into which outfit;
				// when one slot has fewer pieces than the others the first one is
				// reused, so every generated outfit is complete.
				std::set<const RE::TESObjectARMO*> chosen;
				for (const auto& list : bySlot) {
					if (list.empty()) {
						continue;
					}
					chosen.insert(list[std::min(index, list.size() - 1)]);
				}
				std::vector<const RE::TESObjectARMO*> pieces(chosen.begin(), chosen.end());
				if (pieces.empty()) {
					++skipped;
					continue;
				}
				if (service.has_outfit(name.c_str())) {
					auto* outfit = service.get_mut_outfit_ptr(name.c_str());
					if (!outfit) {
						++skipped;
						continue;
					}
					outfit->erase_all_armors();
					for (const auto* piece : pieces) {
						outfit->insert_armor(piece);
					}
					for (std::uint32_t slot = 0; slot < kSlotCount; ++slot) {
						outfit->set_slot_policy_c(slot, nullptr);
					}
					outfit->set_blanket_slot_policy_c(kPolicy);
					outfit->set_favorite(a_favorite);
					++result.overwritten;
					result.names.push_back(name);
				} else {
					if (!service.create_outfit_preset(name, pieces, kPolicy, {}, a_favorite)) {
						++skipped;
						continue;
					}
					++result.created;
					result.names.push_back(name);
				}
				if (index == 0) {
					result.pieces = pieces.size();
				}
			}
		}

		if (result.names.empty()) {
			result.message = "No outfit could be built from that plugin.";
			return result;
		}

		result.ok = true;
		char buffer[320]{};
		std::snprintf(buffer, sizeof(buffer), "%zu outfit(s) from %zu piece(s); %zu created, %zu updated%s%s%s.",
			result.names.size(), result.pieces, result.created, result.overwritten,
			truncated ? "; the plugin holds more alternatives than fit in one run" : "",
			skipped > 0 ? "; some names could not be written" : "",
			notClothing > 0 ? "; FX effects and other non clothing records were left out" : "");
		result.message = buffer;
		SKSE::log::info("ESP outfits: {} ({}; {} wearable of {} slot record(s)).", result.message, a_plugin, wearables.size(), armors.size());
		return result;
	}
}
