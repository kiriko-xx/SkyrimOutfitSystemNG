#include "core/EspOutfits.h"

#include <algorithm>
#include <array>
#include <cctype>
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

namespace SOS::EspOutfits
{
	namespace
	{
		// "Require equipped": the generated outfit only shows on a slot the
		// actor wears something on.
		constexpr const char* kPolicy = "XXXO";
		// A costume with dozens of alternatives would otherwise create dozens of
		// outfits per click; the rest is reported instead of silently dropped.
		constexpr std::size_t kMaxVariants = 32;
		// Hard ceiling for one click, so a plugin with a hundred costumes cannot
		// flood the outfit list.
		constexpr std::size_t kMaxOutfits = 96;
		constexpr std::uint32_t kSlotCount = 32;
		// Index 31 of the display mask is the FX channel (creation kit slot 61).
		// It carries effects, not clothing, so it is never part of a generated
		// outfit even when the record also covers real slots.
		constexpr std::uint32_t kFxBit = 1u << 31;
		// How many leading words two piece names may share and still count as one
		// costume: "Sailor Top" and "Sailor Skirt" share the first one, a plugin
		// that ships "Sailor Top" and "School Top" shares none.
		constexpr std::size_t kMaxNameWords = 4;

		// One word of a piece name: where it starts and how long it is. Words are
		// runs of letters, digits and of bytes >= 0x80, so a Chinese name without
		// spaces stays a single word.
		struct Word
		{
			std::size_t begin{ 0 };
			std::size_t length{ 0 };
		};

		bool IsWordByte(unsigned char a_char)
		{
			return std::isalnum(a_char) != 0 || a_char >= 0x80;
		}

		std::string LowerAscii(std::string a_text)
		{
			for (auto& chr : a_text) {
				chr = static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
			}
			return a_text;
		}

		std::vector<Word> SplitWords(const std::string& a_text)
		{
			std::vector<Word> out;
			std::size_t      i = 0;
			while (i < a_text.size()) {
				if (!IsWordByte(static_cast<unsigned char>(a_text[i]))) {
					++i;
					continue;
				}
				const std::size_t begin = i;
				while (i < a_text.size() && IsWordByte(static_cast<unsigned char>(a_text[i]))) {
					++i;
				}
				out.push_back(Word{ begin, i - begin });
			}
			return out;
		}

		// "[Caenarvon] Sailor Top": the bracketed part names the pack, not the
		// costume, so it is dropped before the names are compared.
		std::vector<Word> NameWords(const std::string& a_text)
		{
			auto             words = SplitWords(a_text);
			std::vector<Word> kept;
			for (const auto& word : words) {
				const std::size_t before = word.begin;
				const std::size_t after = word.begin + word.length;
				const bool        bracketed = before > 0 && (a_text[before - 1] == '[' || a_text[before - 1] == '(')
					&& after < a_text.size() && (a_text[after] == ']' || a_text[after] == ')');
				if (!bracketed) {
					kept.push_back(word);
				}
			}
			return kept;
		}

		// Comparison key of the first a_count words, lower cased and separated by
		// a byte that cannot appear in a name.
		std::string WordKey(const std::string& a_text, const std::vector<Word>& a_words, std::size_t a_count)
		{
			std::string       key;
			const std::size_t count = std::min(a_count, a_words.size());
			for (std::size_t i = 0; i < count; ++i) {
				if (i != 0) {
					key.push_back('\x01');
				}
				const auto& word = a_words[i];
				for (std::size_t k = 0; k < word.length; ++k) {
					key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(a_text[word.begin + k]))));
				}
			}
			return key;
		}

		// The same words in their original spelling, which is what the outfit is
		// named after.
		std::string WordLabel(const std::string& a_text, const std::vector<Word>& a_words, std::size_t a_count)
		{
			const std::size_t count = std::min(a_count, a_words.size());
			if (count == 0) {
				return {};
			}
			const auto& first = a_words.front();
			const auto& last = a_words[count - 1];
			return a_text.substr(first.begin, last.begin + last.length - first.begin);
		}

		struct Piece
		{
			const RE::TESObjectARMO* armor{};
			std::string              name;
			std::vector<Word>        words;
			std::uint32_t            mask{ 0 };
			std::string              family;  // shared name prefix, empty when unique
			std::string              label;   // that prefix in its original spelling
		};

		// Splits one costume into alternatives: a piece joins the first variant that
		// has nothing on its slots yet, otherwise it opens a new variant. Every
		// variant after the first keeps the base look complete, because the pieces
		// of the first variant fill the slots the alternative does not cover.
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

		// 1.5.45: one plugin often ships several costumes (a sailor suit and a
		// school uniform, say). The old rule bucketed the pieces by slot only, so
		// every costume of the plugin was mixed into the same outfit. The pieces
		// are grouped by the words their names start with instead: pieces that
		// share a leading word are the same costume, the odd ones fall into one
		// "rest" costume that keeps the plain plugin name.
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
			piece.words = NameWords(piece.name);
			piece.mask = mask;
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

		// How often every leading word sequence occurs: a sequence used by two
		// pieces or more names a costume, a sequence used once names nothing.
		std::map<std::string, std::size_t> prefixUse;
		for (const auto& piece : pieces) {
			const std::size_t depth = std::min(kMaxNameWords, piece.words.size());
			for (std::size_t count = 1; count <= depth; ++count) {
				++prefixUse[WordKey(piece.name, piece.words, count)];
			}
		}
		for (auto& piece : pieces) {
			const std::size_t depth = std::min(kMaxNameWords, piece.words.size());
			std::size_t       shared = 0;
			for (std::size_t count = 1; count <= depth; ++count) {
				const auto it = prefixUse.find(WordKey(piece.name, piece.words, count));
				if (it != prefixUse.end() && it->second >= 2) {
					shared = count;
				}
			}
			if (shared > 0) {
				piece.family = WordKey(piece.name, piece.words, shared);
				piece.label = WordLabel(piece.name, piece.words, shared);
			}
		}

		std::map<std::string, std::vector<std::size_t>> families;
		for (std::size_t index = 0; index < pieces.size(); ++index) {
			families[pieces[index].family].push_back(index);
		}
		std::vector<std::string> order;
		order.reserve(families.size());
		for (const auto& entry : families) {
			order.push_back(entry.first);
		}
		// The biggest costume first; the pieces without a shared name (the empty
		// key) last, so the plain plugin name always belongs to them.
		std::sort(order.begin(), order.end(), [&families](const std::string& a_lhs, const std::string& a_rhs) {
			if (a_lhs.empty() != a_rhs.empty()) {
				return !a_lhs.empty();
			}
			const auto lhs = families.at(a_lhs).size();
			const auto rhs = families.at(a_rhs).size();
			if (lhs != rhs) {
				return lhs > rhs;
			}
			return a_lhs < a_rhs;
		});

		const std::string base = BaseNameFor(a_plugin);
		// A plugin with more than one costume names the outfits after the costume
		// so they can be told apart in the list; a single costume keeps the plain
		// plugin name, which is what the earlier versions produced.
		const bool multiCostume = order.size() > 1;

		std::set<std::string> usedNames;
		std::size_t           skipped = 0;
		std::size_t           outfits = 0;
		bool                  truncated = false;
		{
			auto  guard = outfit_service_get_mut_singleton_ptr();
			auto& service = guard->inner();
			for (const auto& key : order) {
				const auto& family = families.at(key);
				const auto& firstPiece = pieces[family.front()];
				std::string nameBase = base;
				if (multiCostume && !firstPiece.label.empty()) {
					nameBase += " ";
					nameBase += firstPiece.label;
				}
				const auto variants = BuildVariants(pieces, family, truncated);
				for (std::size_t index = 0; index < variants.size(); ++index) {
					if (outfits >= kMaxOutfits) {
						truncated = true;
						break;
					}
					std::vector<const RE::TESObjectARMO*> chosen;
					chosen.reserve(variants[index].size());
					for (const auto item : variants[index]) {
						chosen.push_back(pieces[item].armor);
					}
					if (chosen.empty()) {
						++skipped;
						continue;
					}
					// Two costumes whose labels happen to collide must not write the
					// same outfit twice.
					std::string name = OutfitNameFor(nameBase, index);
					for (std::size_t suffix = 2; usedNames.find(LowerAscii(name)) != usedNames.end(); ++suffix) {
						name = OutfitNameFor(nameBase, index) + "_" + std::to_string(suffix);
					}
					usedNames.insert(LowerAscii(name));
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
				if (outfits >= kMaxOutfits) {
					break;
				}
			}
		}

		if (result.names.empty()) {
			result.message = "No outfit could be built from that plugin.";
			return result;
		}

		result.ok = true;
		char buffer[420]{};
		std::snprintf(buffer, sizeof(buffer), "%zu outfit(s) from %zu piece(s) in %zu name group(s); %zu created, %zu updated%s%s%s.",
			result.names.size(), result.pieces, order.size(), result.created, result.overwritten,
			truncated ? "; the plugin holds more alternatives than fit in one run" : "",
			skipped > 0 ? "; some names could not be written" : "",
			notClothing > 0 ? "; FX effects and other non clothing records were left out" : "");
		result.message = buffer;
		SKSE::log::info("ESP outfits: {} ({}; {} wearable of {} slot record(s)).", result.message, a_plugin, pieces.size(), armors.size());
		return result;
	}
}
