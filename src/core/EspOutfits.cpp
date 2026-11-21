#include "core/EspOutfits.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/OutfitService.h"
#include "core/PluginScan.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

// 1.5.46: the grouping follows the "prefix tree + longest common prefix + slot
// constraint" design instead of bucketing pieces by the words their names start
// with. Five stages, one per section below:
//   1. the name a comparison runs on (separators never matter, original spelling
//      is kept for the outfit name),
//   2. costume detection: a character level prefix tree, and a branch is split
//      into separate costumes as soon as every branch covers several slots on
//      its own,
//   3. slot buckets inside one costume,
//   4. the differential identifier of every candidate: what is left of the name
//      once the part all candidates of that slot share has been removed,
//   5. combination by identifier, with slot uniqueness enforced and anything
//      ambiguous left alone rather than guessed.

namespace SOS::EspOutfits
{
	namespace
	{
		// "Require equipped": the generated outfit only shows on a slot the
		// actor wears something on.
		constexpr const char* kPolicy = "XXXO";
		// Hard ceiling for one click, so a plugin with a hundred costumes cannot
		// flood the outfit list.
		constexpr std::size_t kMaxOutfits = 96;
		// Ceiling for the loose pieces that share no name with anything else.
		constexpr std::size_t kMaxVariants = 32;
		constexpr std::uint32_t kSlotCount = 32;
		// Index 31 of the display mask is the FX channel (creation kit slot 61).
		// It carries effects, not clothing, so it never becomes part of an outfit
		// even when the record also covers real slots.
		constexpr std::uint32_t kFxBit = 1u << 31;
		constexpr std::size_t kNoPiece = static_cast<std::size_t>(-1);

		// ---------------------------------------------------------------------
		// Stage 1: the name a comparison runs on.
		// ---------------------------------------------------------------------
		// One code point of the original name, plus where it sits in the original
		// bytes so an outfit can still be named with the spelling the user typed.
		// --- stage 1 begin: tests/esp_outfits_trie.ps1 copies this block verbatim ---
		struct Unit
		{
			std::uint32_t codePoint{ 0 };
			std::size_t   begin{ 0 };
			std::size_t   length{ 0 };
		};

		bool IsContinuation(unsigned char a_char)
		{
			return (a_char & 0xC0) == 0x80;
		}

		std::vector<Unit> Decode(const std::string& a_text)
		{
			std::vector<Unit> out;
			std::size_t       i = 0;
			while (i < a_text.size()) {
				const auto    lead = static_cast<unsigned char>(a_text[i]);
				std::size_t   length = 1;
				std::uint32_t codePoint = lead;
				if ((lead & 0x80) != 0) {
					if ((lead & 0xE0) == 0xC0) {
						length = 2;
					} else if ((lead & 0xF0) == 0xE0) {
						length = 3;
					} else if ((lead & 0xF8) == 0xF0) {
						length = 4;
					}
					bool complete = length > 1 && i + length <= a_text.size();
					for (std::size_t k = 1; complete && k < length; ++k) {
						if (!IsContinuation(static_cast<unsigned char>(a_text[i + k]))) {
							complete = false;
						}
					}
					if (complete) {
						std::uint32_t value = static_cast<std::uint32_t>(lead) & (0xFFu >> (length + 1));
						for (std::size_t k = 1; k < length; ++k) {
							value = (value << 6) | (static_cast<unsigned char>(a_text[i + k]) & 0x3Fu);
						}
						codePoint = value;
					} else {
						// A broken byte stays one unit of its own so the offsets into
						// the original string keep adding up.
						length = 1;
						codePoint = 0x110000u + lead;
					}
				}
				out.push_back(Unit{ codePoint, i, length });
				i += length;
			}
			return out;
		}

		// The comparison copy: code points in their original order, ASCII lower
		// cased, every separator (space, underscore, dash, dot, ...) dropped, and a
		// bracketed pack label such as "[Caenarvon]" removed. Because no separator
		// survives, "Sailor_Top", "Sailor-Top" and "Sailor Top" compare equal and
		// the code never has to guess which separator a mod used.
		std::vector<Unit> Normalize(const std::string& a_text)
		{
			const auto        decoded = Decode(a_text);
			std::vector<Unit> out;
			std::size_t       i = 0;
			while (i < decoded.size()) {
				const std::uint32_t codePoint = decoded[i].codePoint;
				if (codePoint == '[' || codePoint == '(') {
					const std::uint32_t close = codePoint == '[' ? ']' : ')';
					std::size_t         j = i + 1;
					while (j < decoded.size() && decoded[j].codePoint != close) {
						++j;
					}
					if (j < decoded.size()) {
						i = j + 1;
						continue;
					}
				}
				if (codePoint < 0x80 && std::isalnum(static_cast<int>(codePoint)) == 0) {
					++i;
					continue;
				}
				Unit unit = decoded[i];
				if (unit.codePoint < 0x80) {
					unit.codePoint = static_cast<std::uint32_t>(std::tolower(static_cast<int>(unit.codePoint)));
				}
				out.push_back(unit);
				++i;
			}
			return out;
		}

		// Original spelling of the units [a_from, a_to). Whole code points only, so
		// a name can never be cut in the middle of a character.
		std::string LabelRange(const std::string& a_original, const std::vector<Unit>& a_units, std::size_t a_from, std::size_t a_to)
		{
			if (a_from >= a_to || a_from >= a_units.size()) {
				return {};
			}
			const std::size_t to = std::min(a_to, a_units.size());
			const std::size_t begin = a_units[a_from].begin;
			const std::size_t end = a_units[to - 1].begin + a_units[to - 1].length;
			if (end <= begin || end > a_original.size()) {
				return {};
			}
			return a_original.substr(begin, end - begin);
		}

		// End index of the part every name in a_names shares, counted from a_from.
		std::size_t CommonEnd(const std::vector<const std::vector<Unit>*>& a_names, std::size_t a_from)
		{
			if (a_names.empty()) {
				return a_from;
			}
			const auto& first = *a_names.front();
			if (a_from >= first.size()) {
				return a_from;
			}
			std::size_t end = first.size();
			for (std::size_t index = 1; index < a_names.size(); ++index) {
				const auto& other = *a_names[index];
				std::size_t k = a_from;
				while (k < first.size() && k < other.size() && first[k].codePoint == other[k].codePoint) {
					++k;
				}
				end = std::min(end, k);
			}
			return end < a_from ? a_from : end;
		}

		std::u32string KeyOf(const std::vector<Unit>& a_units, std::size_t a_from)
		{
			std::u32string key;
			for (std::size_t k = a_from; k < a_units.size(); ++k) {
				key.push_back(static_cast<char32_t>(a_units[k].codePoint));
			}
			return key;
		}

		// --- stage 1 end ---
		struct Piece
		{
			const RE::TESObjectARMO* armor{};
			std::string              name;   // original spelling
			std::vector<Unit>        units;  // comparison copy
			std::uint32_t            mask{ 0 };
			std::size_t              slot{ 0 };  // primary slot: the lowest covered bit
		};

		std::size_t LowestBit(std::uint32_t a_mask)
		{
			for (std::size_t bit = 0; bit < kSlotCount; ++bit) {
				if ((a_mask & (1u << bit)) != 0) {
					return bit;
				}
			}
			return kSlotCount;
		}

		std::size_t DistinctSlots(const std::vector<Piece>& a_pieces, const std::vector<std::size_t>& a_indices)
		{
			std::uint32_t unionMask = 0;
			for (const auto index : a_indices) {
				unionMask |= a_pieces[index].mask;
			}
			std::size_t count = 0;
			for (std::size_t bit = 0; bit < kSlotCount; ++bit) {
				if ((unionMask & (1u << bit)) != 0) {
					++count;
				}
			}
			return count;
		}

		// ---------------------------------------------------------------------
		// Stage 2: costume detection.
		// ---------------------------------------------------------------------
		// Character level prefix tree over the comparison copies. Node 0 is the
		// root; a piece hangs on the node its whole name spells out.
		struct TrieNode
		{
			std::map<std::uint32_t, std::size_t> children;
			std::vector<std::size_t>             pieces;
		};

		std::vector<TrieNode> BuildTrie(const std::vector<Piece>& a_pieces)
		{
			std::vector<TrieNode> nodes(1);
			for (std::size_t index = 0; index < a_pieces.size(); ++index) {
				std::size_t node = 0;
				for (const auto& unit : a_pieces[index].units) {
					const auto it = nodes[node].children.find(unit.codePoint);
					if (it == nodes[node].children.end()) {
						nodes.push_back(TrieNode{});
						const std::size_t next = nodes.size() - 1;
						nodes[node].children.emplace(unit.codePoint, next);
						node = next;
					} else {
						node = it->second;
					}
				}
				nodes[node].pieces.push_back(index);
			}
			return nodes;
		}

		std::vector<std::size_t> SubtreePieces(const std::vector<TrieNode>& a_nodes, std::size_t a_node)
		{
			std::vector<std::size_t> out;
			std::vector<std::size_t> stack{ a_node };
			while (!stack.empty()) {
				const std::size_t node = stack.back();
				stack.pop_back();
				for (const auto index : a_nodes[node].pieces) {
					out.push_back(index);
				}
				for (const auto& child : a_nodes[node].children) {
					stack.push_back(child.second);
				}
			}
			return out;
		}

		// One costume: the shared prefix (both spellings) and its pieces.
		struct Costume
		{
			std::vector<std::uint32_t> prefix;  // comparison copy of the shared prefix
			std::string                label;   // that prefix in its original spelling
			std::vector<std::size_t>   pieces;
		};

		// Walks the tree and decides where one costume ends and the next begins.
		// A branch stays one costume while the pieces under it do not each form a
		// multi slot costume of their own: "Sailor hair / Sailor top / Sailor
		// skirt" splits into hair, top and skirt on the last level, and none of
		// those covers several slots, so the branch is one costume. Two costumes
		// that merely share a short prefix ("Star night dress ..." and "Star
		// evening dress ...") do branch into parts that each cover the costume's
		// slots, so they are split.
		// Pieces that cannot form a costume of their own (a single slot, a single
		// record) are collected separately and keep the plain plugin name.
		void Classify(const std::vector<TrieNode>& a_nodes, const std::vector<Piece>& a_pieces, std::size_t a_node, std::vector<std::uint32_t>& a_prefix, std::vector<Costume>& a_costumes, std::vector<std::size_t>& a_loose)
		{
			const auto pieces = SubtreePieces(a_nodes, a_node);
			if (pieces.size() < 2 || DistinctSlots(a_pieces, pieces) < 2) {
				a_loose.insert(a_loose.end(), pieces.begin(), pieces.end());
				return;
			}
			if (!a_nodes[a_node].children.empty()) {
				std::size_t branches = 0;
				bool        everyBranchIsACostume = true;
				for (const auto& child : a_nodes[a_node].children) {
					++branches;
					const auto childPieces = SubtreePieces(a_nodes, child.second);
					if (childPieces.size() < 2 || DistinctSlots(a_pieces, childPieces) < 2) {
						everyBranchIsACostume = false;
					}
				}
				// Only split when the branches really are separate costumes; a single
				// child just means the name continues, so the walk goes deeper
				// without splitting.
				if (branches > 1 && everyBranchIsACostume) {
					for (const auto& child : a_nodes[a_node].children) {
						a_prefix.push_back(child.first);
						Classify(a_nodes, a_pieces, child.second, a_prefix, a_costumes, a_loose);
						a_prefix.pop_back();
					}
					return;
				}
			}
			Costume costume;
			costume.prefix = a_prefix;
			costume.pieces = pieces;
			std::sort(costume.pieces.begin(), costume.pieces.end());
			const auto& representative = a_pieces[costume.pieces.front()];
			costume.label = LabelRange(representative.name, representative.units, 0, a_prefix.size());
			a_costumes.push_back(std::move(costume));
		}

		// ---------------------------------------------------------------------
		// Stages 3-5: slot buckets, differential identifiers, combinations.
		// ---------------------------------------------------------------------
		struct Preset
		{
			std::vector<std::size_t> pieces;
			std::uint32_t            mask{ 0 };
		};

		struct SlotPlan
		{
			std::size_t                           only{ kNoPiece };  // exactly one candidate
			std::map<std::u32string, std::size_t> byKey;             // identifier -> piece
			std::size_t                           duplicates{ 0 };
		};

		struct CostumePlan
		{
			std::vector<Preset> presets;
			std::size_t         duplicates{ 0 };
			std::size_t         clashes{ 0 };
			std::size_t         unmatched{ 0 };
		};

		// A costume's pieces are bucketed by slot. Inside one bucket the part every
		// candidate shares is removed; whatever is left is the differential
		// identifier ("A", "B", "red", "black"). Identifiers are then matched across
		// slots: a slot with a single candidate is shared by every preset, a slot
		// with several has to hold the piece that carries the current identifier.
		CostumePlan PlanCostume(const std::vector<Piece>& a_pieces, const Costume& a_costume)
		{
			CostumePlan plan;

			std::map<std::size_t, std::vector<std::size_t>> buckets;
			for (const auto index : a_costume.pieces) {
				buckets[a_pieces[index].slot].push_back(index);
			}

			std::map<std::size_t, SlotPlan>       plans;
			std::set<std::u32string>              identifiers;
			std::vector<const std::vector<Unit>*> names;
			for (auto& bucket : buckets) {
				auto& candidates = bucket.second;
				std::sort(candidates.begin(), candidates.end());
				SlotPlan slotPlan;
				if (candidates.size() == 1) {
					slotPlan.only = candidates.front();
					plans.emplace(bucket.first, std::move(slotPlan));
					continue;
				}
				names.clear();
				for (const auto index : candidates) {
					names.push_back(&a_pieces[index].units);
				}
				const std::size_t sharedEnd = CommonEnd(names, a_costume.prefix.size());
				for (const auto index : candidates) {
					const auto& units = a_pieces[index].units;
					const std::size_t from = std::min(sharedEnd, units.size());
					auto key = KeyOf(units, from);
					const auto inserted = slotPlan.byKey.emplace(key, index);
					if (!inserted.second) {
						++slotPlan.duplicates;
						continue;
					}
					identifiers.insert(std::move(key));
				}
				plan.duplicates += slotPlan.duplicates;
				plans.emplace(bucket.first, std::move(slotPlan));
			}

			// 0 = written, 1 = a slot has no candidate for that identifier,
			// 2 = two pieces of the combination cover the same slot.
			const auto emit = [&](const std::map<std::size_t, std::size_t>& a_choice) {
				Preset        preset;
				std::uint32_t mask = 0;
				for (const auto& slot : plans) {
					std::size_t index = slot.second.only;
					if (index == kNoPiece) {
						const auto it = a_choice.find(slot.first);
						if (it == a_choice.end()) {
							return 1;
						}
						index = it->second;
					}
					if (index >= a_pieces.size()) {
						return 1;
					}
					const std::uint32_t pieceMask = a_pieces[index].mask;
					if ((mask & pieceMask) != 0) {
						return 2;
					}
					mask |= pieceMask;
					preset.pieces.push_back(index);
				}
				if (preset.pieces.empty()) {
					return 1;
				}
				std::sort(preset.pieces.begin(), preset.pieces.end());
				preset.mask = mask;
				plan.presets.push_back(std::move(preset));
				return 0;
			};

			if (identifiers.empty()) {
				// Every slot has exactly one candidate: one preset, nothing to vary.
				const auto code = emit({});
				if (code == 2) {
					++plan.clashes;
				} else if (code != 0) {
					++plan.unmatched;
				}
				return plan;
			}

			for (const auto& identifier : identifiers) {
				std::map<std::size_t, std::size_t> choice;
				bool                               missing = false;
				for (const auto& slot : plans) {
					if (slot.second.only != kNoPiece) {
						continue;
					}
					const auto it = slot.second.byKey.find(identifier);
					if (it == slot.second.byKey.end()) {
						missing = true;
						break;
					}
					choice.emplace(slot.first, it->second);
				}
				if (missing) {
					// "top A/B" next to "skirt red/black": the identifiers do not
					// correspond, so guessing which top goes with which skirt would
					// produce a preset that looks complete and is wrong. Leave it.
					++plan.unmatched;
					continue;
				}
				const auto code = emit(choice);
				if (code == 2) {
					++plan.clashes;
				} else if (code != 0) {
					++plan.unmatched;
				}
			}
			return plan;
		}

		// Pieces that share no name with anything else keep the plain plugin name.
		// They are split by slot conflicts only: a piece joins the first variant
		// that has nothing on its slots yet, and every later variant is completed
		// with the base pieces so it still covers the whole look.
		std::vector<std::vector<std::size_t>> BuildVariants(const std::vector<Piece>& a_pieces, const std::vector<std::size_t>& a_family, bool& a_truncated)
		{
			std::vector<std::vector<std::size_t>> variants;
			std::vector<std::uint32_t>            masks;
			for (const auto index : a_family) {
				const auto& piece = a_pieces[index];
				bool        placed = false;
				for (std::size_t variant = 0; variant < variants.size(); ++variant) {
					if ((masks[variant] & piece.mask) == 0) {
						variants[variant].push_back(index);
						masks[variant] |= piece.mask;
						placed = true;
						break;
					}
				}
				if (placed) {
					continue;
				}
				if (variants.size() >= kMaxVariants) {
					a_truncated = true;
					continue;
				}
				variants.push_back(std::vector<std::size_t>{ index });
				masks.push_back(piece.mask);
			}
			if (variants.empty()) {
				return variants;
			}
			const auto base = variants.front();
			for (std::size_t variant = 1; variant < variants.size(); ++variant) {
				for (const auto index : base) {
					if ((masks[variant] & a_pieces[index].mask) == 0) {
						variants[variant].push_back(index);
						masks[variant] |= a_pieces[index].mask;
					}
				}
			}
			return variants;
		}
	}// namespace

	std::vector<PluginSummary> PluginsWithArmor()
	{
		std::vector<PluginSummary> out;
		for (const auto& entry : PluginScan::PluginsWithArmor()) {
			// The list shows every plugin that carries slot armour; a plugin whose
			// pieces all fail the clothing test is still worth showing, because that
			// is how the user finds out what the plugin holds.
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

		// Only ordinary clothing becomes an outfit: an FX effect, a record without
		// an editor id and a record without a model are all skipped, and the count
		// is reported so the list can show how many pieces a plugin really offers.
		// Golden items do not do this: they take any record.
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

		std::vector<Piece> pieces;
		pieces.reserve(wearables.size());
		for (const auto* armor : wearables) {
			if (!armor) {
				continue;
			}
			const auto mask = armor->GetSlotMask().underlying() & ~kFxBit;
			if (mask == 0) {
				continue;
			}
			Piece piece;
			piece.armor = armor;
			const char* full = armor->GetFullName();
			const char* editor = armor->GetFormEditorID();
			if (full && full[0] != '\0') {
				piece.name = full;
			} else if (editor && editor[0] != '\0') {
				piece.name = editor;
			}
			piece.units = Normalize(piece.name);
			piece.mask = mask;
			piece.slot = LowestBit(mask);
			pieces.push_back(std::move(piece));
		}
		if (pieces.empty()) {
			result.message = "That plugin holds no wearable clothing that covers a display slot.";
			return result;
		}

		// A fixed order keeps the generated names the same on every run.
		std::sort(pieces.begin(), pieces.end(), [](const Piece& a_lhs, const Piece& a_rhs) {
			if (a_lhs.name != a_rhs.name) {
				return a_lhs.name < a_rhs.name;
			}
			return a_lhs.armor->GetFormID() < a_rhs.armor->GetFormID();
		});

		// Stage 2: which pieces belong to the same costume.
		const auto            trie = BuildTrie(pieces);
		std::vector<Costume>  costumes;
		std::vector<std::size_t> loose;
		{
			std::vector<std::uint32_t> prefix;
			Classify(trie, pieces, 0, prefix, costumes, loose);
		}
		// The biggest costume first, so the plain plugin name never goes to a
		// leftover record when a real costume exists.
		std::sort(costumes.begin(), costumes.end(), [](const Costume& a_lhs, const Costume& a_rhs) {
			if (a_lhs.pieces.size() != a_rhs.pieces.size()) {
				return a_lhs.pieces.size() > a_rhs.pieces.size();
			}
			return a_lhs.label < a_rhs.label;
		});
		if (!loose.empty()) {
			Costume rest;
			rest.pieces = loose;
			std::sort(rest.pieces.begin(), rest.pieces.end());
			costumes.push_back(std::move(rest));
		}

		const std::string base = BaseNameFor(a_plugin);
		// A plugin with more than one costume names the outfits after the costume so
		// they can be told apart in the list; a single costume keeps the plain
		// plugin name, which is what the earlier versions produced.
		const bool multiCostume = costumes.size() > 1;

		std::set<std::string> usedNames;
		std::size_t           skipped = 0;
		std::size_t           outfits = 0;
		std::size_t           unmatched = 0;
		bool                  truncated = false;
		{
			auto  guard = outfit_service_get_mut_singleton_ptr();
			auto& service = guard->inner();
			for (const auto& costume : costumes) {
				if (outfits >= kMaxOutfits) {
					truncated = true;
					break;
				}
				std::vector<Preset> presets;
				if (costume.label.empty()) {
					// The loose records: no shared name to work with, so they are only
					// split by slot conflicts.
					const auto variants = BuildVariants(pieces, costume.pieces, truncated);
					for (const auto& variant : variants) {
						Preset preset;
						for (const auto index : variant) {
							preset.mask |= pieces[index].mask;
							preset.pieces.push_back(index);
						}
						presets.push_back(std::move(preset));
					}
				} else {
					auto plan = PlanCostume(pieces, costume);
					unmatched += plan.unmatched;
					if (plan.duplicates > 0) {
						SKSE::log::info("ESP outfits: costume '{}' holds {} record(s) with the same name and slot; the first one is used.", costume.label, plan.duplicates);
					}
					if (plan.clashes > 0) {
						SKSE::log::warn("ESP outfits: costume '{}' holds {} combination(s) that would wear two pieces on one slot; they were left out.", costume.label, plan.clashes);
					}
					if (plan.presets.empty()) {
						SKSE::log::warn("ESP outfits: the alternatives of costume '{}' cannot be matched by name across slots ({} unmatched); no preset was written for it so no wrong outfit appears in the list.", costume.label, plan.unmatched);
						continue;
					}
					presets = std::move(plan.presets);
				}
				std::string nameBase = base;
				if (multiCostume && !costume.label.empty()) {
					nameBase += " ";
					nameBase += costume.label;
				}
				for (std::size_t index = 0; index < presets.size(); ++index) {
					if (outfits >= kMaxOutfits) {
						truncated = true;
						break;
					}
					std::vector<const RE::TESObjectARMO*> chosen;
					chosen.reserve(presets[index].pieces.size());
					for (const auto item : presets[index].pieces) {
						chosen.push_back(pieces[item].armor);
					}
					if (chosen.empty()) {
						++skipped;
						continue;
					}
					// Two costumes whose labels happen to collide must not write the
					// same outfit twice.
					std::string name = OutfitNameFor(nameBase, index);
					for (std::size_t suffix = 2; usedNames.find(name) != usedNames.end(); ++suffix) {
						name = OutfitNameFor(nameBase, index) + "_" + std::to_string(suffix);
					}
					usedNames.insert(name);
					++outfits;
					if (service.has_outfit(name.c_str())) {
						auto* outfit = service.get_mut_outfit_ptr(name.c_str());
						if (!outfit) {
							++skipped;
							continue;
						}
						outfit->erase_all_armors();
						for (const auto* piece : chosen) {
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
						if (!service.create_outfit_preset(name, chosen, kPolicy, {}, a_favorite)) {
							++skipped;
							continue;
						}
						++result.created;
						result.names.push_back(name);
					}
					if (result.pieces == 0) {
						result.pieces = chosen.size();
					}
				}
			}
		}

		if (result.names.empty()) {
			result.message = "No outfit could be built from that plugin.";
			return result;
		}

		result.ok = true;
		char buffer[480]{};
		std::snprintf(buffer, sizeof(buffer), "%zu outfit(s) from %zu piece(s) in %zu costume(s); %zu created, %zu updated%s%s%s%s.",
			result.names.size(), result.pieces, costumes.size(), result.created, result.overwritten,
			truncated ? "; the plugin holds more alternatives than fit in one run" : "",
			skipped > 0 ? "; some names could not be written" : "",
			notClothing > 0 ? "; FX effects and other non clothing records were left out" : "",
			unmatched > 0 ? "; some alternatives did not match across slots and were left out" : "");
		result.message = buffer;
		SKSE::log::info("ESP outfits: {} ({}; {} wearable of {} slot record(s)).", result.message, a_plugin, pieces.size(), armors.size());
		for (const auto& costume : costumes) {
			SKSE::log::debug("ESP outfits: costume '{}' holds {} piece(s) over {} slot(s).", costume.label.empty() ? std::string{ "(no shared name)" } : costume.label, costume.pieces.size(), DistinctSlots(pieces, costume.pieces));
		}
		return result;
	}
}
