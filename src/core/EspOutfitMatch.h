#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Pure planning: indexes refer to the caller's sorted records; no game or storage access.
namespace SOS::EspOutfits::Match
{
	// The FX bit marks a visual effect, not a body slot: two parts may both carry
	// one without fighting over the same body part.
	inline constexpr std::uint32_t kFxBit = 1u << 31;
	inline constexpr std::size_t kMaxVariants = 32;
	inline constexpr std::size_t kMaxOutfits = 96;
	using IndexList = std::vector<std::size_t>;
	using Key = std::u32string;
	using Aliases = std::map<Key, Key>;  // explicit, already normalized; never infer translations

	inline std::uint32_t BodySlots(std::uint32_t mask)
	{
		return mask & ~kFxBit;
	}

	// Picking which of several pieces fills an empty slot has to be a draw, not
	// plugin order: five presets of one style must not all wear the same panty.
	// splitmix64 keeps the draw deterministic per seed and identical on every build,
	// so a report can be reproduced exactly by reusing its seed.
	struct Rand
	{
		std::uint64_t state{};

		explicit Rand(std::uint64_t seed = 0) : state(seed) {}

		std::uint64_t Next()
		{
			std::uint64_t z = (state += 0x9E3779B97F4A7C15ull);
			z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
			z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
			return z ^ (z >> 31);
		}

		// Uniform enough for choosing between a handful of options.
		std::size_t Pick(std::size_t count)
		{
			return count > 1 ? static_cast<std::size_t>(Next() % count) : 0;
		}
	};

	struct Piece
	{
		Key key;
		std::uint32_t mask{};
	};

	struct Group
	{
		Key prefix;
		IndexList members;
		std::vector<IndexList> variants;
		const char* issue{};
		std::uint32_t coverage{};
		// Pieces this family pairs by name alone, before slots are completed.
		std::size_t aligned{};
		// Pieces this family could not place, with the reason the menu shows.
		std::vector<std::pair<std::size_t, const char*>> pending;
	};

	struct Node
	{
		std::map<char32_t, std::size_t> children;
		IndexList members;
		bool complementary{};
	};

	inline bool FamilyEvidence(const Node& node, std::u32string_view prefix)
	{
		// Short shared ASCII fragments (e.g. Da in Dark/Dawn) are weak evidence.
		// Non-ASCII languages need no word tokenizer; one code point is still too weak.
		const bool ascii = std::all_of(prefix.begin(), prefix.end(), [](char32_t ch) { return ch < 128; });
		return node.complementary && prefix.size() >= (ascii ? 3u : 2u);
	}

	inline std::size_t PlacedOf(const std::vector<Piece>& pieces, const std::vector<IndexList>& variants)
	{
		(void)pieces;
		IndexList all;
		for (const auto& variant : variants) {
			all.insert(all.end(), variant.begin(), variant.end());
		}
		std::sort(all.begin(), all.end());
		all.erase(std::unique(all.begin(), all.end()), all.end());
		return all.size();
	}

	// Every preset of a style has to fill every slot that style offers. A slot that
	// only some variants cover is completed from the family itself - a shared part,
	// or one option of that same slot - so a style never ends up half dressed.
	// An empty slot is filled with exactly one piece: the player wants one outfit per
	// style, not the whole cross product of every option in every slot. Which piece
	// gets the slot is drawn (see Rand), so the five presets of one style do not all
	// wear the same option. Without a Rand the first candidate in plugin order is used,
	// which keeps the pure planner reproducible for tests.
	// A piece that only offers another option for a slot that is already filled still
	// gets a preset of its own: one existing preset has that slot swapped to it. That
	// keeps every record the player owns reachable, and the preset count grows by one
	// per such piece instead of by a cross product.
	inline void FinalizeVariants(Group& group, const std::vector<Piece>& pieces, Rand* rng = nullptr)
	{
		std::uint32_t coverage = 0;
		for (const auto i : group.members) {
			coverage |= BodySlots(pieces[i].mask);
		}
		group.coverage = coverage;
		const auto coveredBy = [&pieces](const IndexList& variant) {
			std::uint32_t mask = 0;
			for (const auto i : variant) {
				mask |= BodySlots(pieces[i].mask);
			}
			return mask;
		};
		std::vector<IndexList>                         kept;
		std::vector<std::pair<IndexList, const char*>> dead;
		std::vector<std::size_t>                       overflowed;
		for (auto& variant : group.variants) {
			if (kept.size() >= kMaxVariants) {
				for (const auto i : variant) {
					overflowed.push_back(i);
				}
				continue;
			}
			auto missing = coverage & ~coveredBy(variant);
			while (missing != 0) {
				// Widest coverage first keeps the fill possible; among the pieces that
				// fill equally much the draw is free. Panty and Panty (Trans) both take
				// one slot, so either of them may end up in this preset.
				std::size_t              bestCount = 0;
				std::vector<std::size_t> best;
				for (const auto i : group.members) {
					if (std::find(variant.begin(), variant.end(), i) != variant.end()) {
						continue;
					}
					const auto slots = BodySlots(pieces[i].mask);
					if (slots == 0 || (slots & ~missing) != 0) {
						continue;
					}
					const auto count = static_cast<std::size_t>(std::popcount(slots));
					if (count > bestCount) {
						bestCount = count;
						best.assign(1, i);
					} else if (count == bestCount) {
						best.push_back(i);
					}
				}
				if (best.empty()) {
					break;
				}
				const auto chosen = best[rng ? rng->Pick(best.size()) : 0];
				variant.push_back(chosen);
				missing &= ~BodySlots(pieces[chosen].mask);
			}
			std::sort(variant.begin(), variant.end());
			if (missing != 0) {
				dead.push_back({ std::move(variant), "incomplete-slots" });
			} else if (variant.size() >= 2) {
				kept.push_back(std::move(variant));
			} else {
				dead.push_back({ std::move(variant), "no-complete-outfit" });
			}
		}
		std::sort(kept.begin(), kept.end());
		kept.erase(std::unique(kept.begin(), kept.end()), kept.end());
		const auto used = [&kept](std::size_t index) {
			return std::any_of(kept.begin(), kept.end(), [index](const IndexList& variant) {
				return std::binary_search(variant.begin(), variant.end(), index);
			});
		};
		// One swap-in preset per still unused piece, never one per combination. With no
		// preset to swap in yet there is nothing to build on, and the dead reasons below
		// explain the style better than "that slot is taken".
		const auto                       bases = kept;
		std::vector<std::size_t>         tooMany;
		std::vector<std::size_t>         noSwap;
		for (const auto i : group.members) {
			if (bases.empty()) {
				break;
			}
			if (used(i) || BodySlots(pieces[i].mask) == 0) {
				continue;
			}
			if (kept.size() >= kMaxVariants) {
				tooMany.push_back(i);
				continue;
			}
			const auto slots = BodySlots(pieces[i].mask);
			const auto start = rng ? rng->Pick(bases.size()) : std::size_t{ 0 };
			bool       accepted = false;
			for (std::size_t k = 0; k < bases.size() && !accepted; ++k) {
				const auto& base = bases[start + k < bases.size() ? start + k : start + k - bases.size()];
				IndexList   next;
				bool        swapped = false;
				for (const auto j : base) {
					if ((BodySlots(pieces[j].mask) & slots) != 0) {
						swapped = true;
						continue;
					}
					next.push_back(j);
				}
				if (!swapped) {
					continue;
				}
				next.push_back(i);
				std::sort(next.begin(), next.end());
				if (next.size() < 2 || coveredBy(next) != coverage || std::find(kept.begin(), kept.end(), next) != kept.end()) {
					continue;
				}
				kept.push_back(next);
				accepted = true;
			}
			if (!accepted) {
				noSwap.push_back(i);
			}
		}
		std::vector<std::pair<std::size_t, const char*>> pending;
		const auto note = [&pending](std::size_t index, const char* why) {
			if (std::none_of(pending.begin(), pending.end(), [index](const auto& entry) { return entry.first == index; })) {
				pending.push_back({ index, why });
			}
		};
		// A piece that conflicts with the others is the real reason a style stays
		// empty, so those remarks come before the ones about a slot being taken.
		for (const auto& [index, why] : group.pending) {
			if (!used(index) && std::string_view(why) != "unmatched-difference") {
				note(index, why);
			}
		}
		for (const auto i : noSwap) {
			if (!used(i)) {
				note(i, "same-slot-alternative");
			}
		}
		for (const auto i : tooMany) {
			if (!used(i)) {
				note(i, "variant-limit");
			}
		}
		for (const auto i : overflowed) {
			if (!used(i)) {
				note(i, "variant-limit");
			}
		}
		for (const auto& [index, why] : group.pending) {
			if (!used(index)) {
				note(index, why);
			}
		}
		for (const auto& [variant, why] : dead) {
			for (const auto i : variant) {
				if (!used(i)) {
					note(i, why);
				}
			}
		}
		for (const auto i : group.members) {
			if (!used(i)) {
				note(i, "no-complete-outfit");
			}
		}

		std::sort(pending.begin(), pending.end());
		group.pending = std::move(pending);
		group.variants = std::move(kept);
	}

	// Turns one family into presets. Every preset holds at most one piece per slot,
	// and several pieces on one slot become variants only when their difference
	// marks line up. Nothing is paired by position and nothing is invented, yet one
	// unreadable slot no longer sinks the whole outfit.
	inline void MatchVariants(Group& group, const std::vector<Piece>& pieces, const Aliases& aliases, Rand* rng = nullptr)
	{
		std::map<std::uint32_t, IndexList> buckets;
		for (const auto i : group.members) {
			buckets[pieces[i].mask].push_back(i);
		}
		std::vector<std::uint32_t> order;
		order.reserve(buckets.size());
		for (const auto& [mask, list] : buckets) {
			order.push_back(mask);
		}
		// Narrow pieces first: a piece that covers several slots must never push
		// aside the ordinary parts of the outfit.
		std::sort(order.begin(), order.end(), [](std::uint32_t lhs, std::uint32_t rhs) {
			const auto left = std::popcount(lhs);
			const auto right = std::popcount(rhs);
			return left != right ? left < right : lhs < rhs;
		});
		IndexList                                         shared;
		std::vector<std::map<Key, std::size_t>>            choices;
		std::uint32_t                                      occupied = 0;
		for (const auto mask : order) {
			auto& bucket = buckets.at(mask);
			if ((occupied & BodySlots(mask)) != 0) {
				for (const auto i : bucket) {
					group.pending.push_back({ i, "overlapping-slot-masks" });
				}
				continue;
			}
			occupied |= mask;
			if (bucket.size() == 1) {
				shared.push_back(bucket.front());
				continue;
			}
			std::sort(bucket.begin(), bucket.end(), [&](auto lhs, auto rhs) { return pieces[lhs].key < pieces[rhs].key; });
			for (std::size_t i = kMaxVariants; i < bucket.size(); ++i) {
				group.pending.push_back({ bucket[i], "variant-limit" });
			}
			if (bucket.size() > kMaxVariants) {
				bucket.resize(kMaxVariants);
			}
			const std::u32string_view first = pieces[bucket.front()].key;
			std::size_t               common = first.size();
			for (const auto i : bucket) {
				const std::u32string_view other = pieces[i].key;
				common = static_cast<std::size_t>(
					std::mismatch(first.begin(), first.begin() + common, other.begin(), other.end()).first - first.begin());
			}
			std::map<Key, std::size_t> tags;
			std::map<Key, std::size_t> counts;
			std::map<std::size_t, Key> tagOf;
			for (const auto i : bucket) {
				Key tag = pieces[i].key.substr(common);
				if (!tag.empty()) {
					if (const auto alias = aliases.find(tag); alias != aliases.end()) {
						tag = alias->second;
					}
				}
				++counts[tag];
				tagOf.emplace(i, tag);
			}
			// A single empty mark is the base variant ("Panty" next to "Panty (Trans)");
			// only an actually repeated mark is unprovable, and only those pieces drop.
			for (const auto& [i, tag] : tagOf) {
				if (counts[tag] == 1) {
					tags.emplace(tag, i);
				} else {
					group.pending.push_back({ i, "non-unique-difference" });
				}
			}
			if (tags.empty()) {
				continue;
			}
			choices.push_back(std::move(tags));
		}
		if (choices.empty()) {
			std::sort(shared.begin(), shared.end());
			if (shared.size() >= 2) {
				group.variants.push_back(std::move(shared));
			} else {
				group.pending.push_back({ shared.front(), "no-complete-outfit" });
			}
			group.aligned = PlacedOf(pieces, group.variants);
			FinalizeVariants(group, pieces, rng);
			if (group.variants.empty()) {
				group.issue = !group.pending.empty() ? group.pending.front().second : "no-complete-outfit";
			}
			return;
		}
		// Slots whose difference marks agree form one variant series; a wider series
		// (more slots, then more marks) names the presets, and the other slots join
		// whenever their mark matches exactly. Marks that match nothing stay listed.
		std::map<std::vector<Key>, std::vector<std::size_t>> classes;
		for (std::size_t i = 0; i < choices.size(); ++i) {
			std::vector<Key> tags;
			tags.reserve(choices[i].size());
			for (const auto& [tag, index] : choices[i]) {
				tags.push_back(tag);
			}
			classes[tags].push_back(i);
		}
		struct Series
		{
			std::vector<Key>         tags;
			std::vector<std::size_t> slots;
		};
		std::vector<Series> series;
		series.reserve(classes.size());
		for (auto& [tags, slots] : classes) {
			series.push_back({ std::move(tags), std::move(slots) });
		}
		std::sort(series.begin(), series.end(), [](const Series& lhs, const Series& rhs) {
			if (lhs.slots.size() != rhs.slots.size()) {
				return lhs.slots.size() > rhs.slots.size();
			}
			if (lhs.tags.size() != rhs.tags.size()) {
				return lhs.tags.size() > rhs.tags.size();
			}
			return lhs.tags < rhs.tags;
		});
		const auto& primary = series.front();
		std::map<std::pair<Key, std::size_t>, std::size_t> optional;
		for (std::size_t s = 1; s < series.size(); ++s) {
			for (const auto slot : series[s].slots) {
				for (const auto& [tag, index] : choices[slot]) {
					if (std::binary_search(primary.tags.begin(), primary.tags.end(), tag)) {
						optional.emplace(std::make_pair(tag, slot), index);
					} else {
						group.pending.push_back({ index, "unmatched-difference" });
					}
				}
			}
		}
		std::vector<IndexList> built;
		for (const auto& tag : primary.tags) {
			IndexList variant = shared;
			for (const auto slot : primary.slots) {
				variant.push_back(choices[slot].at(tag));
			}
			for (std::size_t s = 1; s < series.size(); ++s) {
				for (const auto slot : series[s].slots) {
					if (const auto found = optional.find(std::make_pair(tag, slot)); found != optional.end()) {
						variant.push_back(found->second);
					}
				}
			}
			std::sort(variant.begin(), variant.end());
			built.push_back(std::move(variant));
		}
		group.variants = std::move(built);
		group.aligned = PlacedOf(pieces, group.variants);
		const char* alignedReason = group.pending.empty() ? nullptr : group.pending.front().second;
		FinalizeVariants(group, pieces, rng);
		if (group.variants.empty()) {
			group.issue = alignedReason ? alignedReason : (!group.pending.empty() ? group.pending.front().second : "no-complete-outfit");
		}
	}

	// Every closed name prefix that could name a family, widest first is not assumed:
	// the planner below scores them. A prefix is evidence when its pieces cover
	// complementary slots; the length floor keeps accidental short fragments out
	// (three characters for ASCII, two code points elsewhere).
	template <class KeyOf>
	inline std::vector<std::pair<Key, IndexList>> CandidateFamilies(const std::vector<Piece>& pieces, const IndexList& avail, KeyOf keyOf)
	{
		std::vector<Node> trie(1);
		for (const auto i : avail) {
			const auto key = keyOf(i);
			const auto mask = pieces[i].mask;
			if (key.empty() || !mask || key.size() > 256) {
				continue;
			}
			std::size_t node = 0;
			for (const auto ch : key) {
				auto [entry, inserted] = trie[node].children.try_emplace(ch, trie.size());
				node = entry->second;
				if (inserted) {
					trie.emplace_back();
				}
				auto& current = trie[node];
				if (!current.complementary) {
					current.complementary = std::any_of(current.members.begin(), current.members.end(), [&](auto j) {
						return (BodySlots(pieces[j].mask) & BodySlots(mask)) == 0;
					});
				}
				current.members.push_back(i);
			}
		}
		std::vector<std::pair<Key, IndexList>> out;
		Key                                    prefix;
		const auto                             walk = [&](auto&& self, std::size_t index) -> void {
			const auto& node = trie[index];
			if (FamilyEvidence(node, prefix)) {
				out.emplace_back(prefix, node.members);
			}
			for (const auto& [ch, child] : node.children) {
				prefix.push_back(ch);
				self(self, child);
				prefix.pop_back();
			}
		};
		walk(walk, 0);
		return out;
	}

	inline std::size_t PlacedPieces(const Group& group)
	{
		IndexList all;
		for (const auto& variant : group.variants) {
			all.insert(all.end(), variant.begin(), variant.end());
		}
		std::sort(all.begin(), all.end());
		all.erase(std::unique(all.begin(), all.end()), all.end());
		return all.size();
	}

	// A family is only worth taking when it actually pairs pieces, it explains as
	// many pieces as possible, and it leaves as few as possible unexplained.
	// Pairing by name outranks filling slots: two styles under one shared prefix
	// must not beat their own two families just because a cross of both fills more.
	inline bool Better(const Group& lhs, std::size_t lhsPlaced, const Group& rhs, std::size_t rhsPlaced)
	{
		if (lhs.aligned != rhs.aligned) {
			return lhs.aligned > rhs.aligned;
		}
		if (lhsPlaced != rhsPlaced) {
			return lhsPlaced > rhsPlaced;
		}
		if (lhs.members.size() != rhs.members.size()) {
			return lhs.members.size() > rhs.members.size();
		}
		if (lhs.pending.size() != rhs.pending.size()) {
			return lhs.pending.size() < rhs.pending.size();
		}
		return lhs.prefix.size() > rhs.prefix.size();
	}

	// Splits one set of pieces into families. The widest usable name prefix wins on
	// even standing, but a wider prefix that pairs fewer pieces loses: two styles
	// under a shared prefix ("星夜礼服" and "星夜晚礼服" under "星夜") stay apart
	// instead of being matched across styles.
	template <class KeyOf>
	inline std::vector<Group> FamiliesOf(const std::vector<Piece>& pieces, const Aliases& aliases, IndexList avail, KeyOf keyOf, IndexList& leftover, bool a_report, Rand* rng = nullptr)
	{
		std::vector<Group> families;
		while (avail.size() >= 2) {
			Group      best;
			std::size_t bestPlaced = 0;
			bool        found = false;
			for (const auto& [prefix, members] : CandidateFamilies(pieces, avail, keyOf)) {
				Group group;
				group.prefix = prefix;
				group.members = members;
				MatchVariants(group, pieces, aliases);
				const auto placed = PlacedPieces(group);
				if (!found || Better(group, placed, best, bestPlaced)) {
					best = std::move(group);
					bestPlaced = placed;
					found = true;
				}
			}
			if (!found || (bestPlaced == 0 && !a_report)) {
				break;
			}
			if (rng) {
				// Scoring above ran with no draw so the candidates stay comparable; the
				// family that wins is laid out again on the caller's stream, which gives
				// every family its own picks.
				Group replay;
				replay.prefix = best.prefix;
				replay.members = best.members;
				MatchVariants(replay, pieces, aliases, rng);
				best = std::move(replay);
			}
			IndexList gone = best.members;
			std::sort(gone.begin(), gone.end());
			IndexList rest;
			rest.reserve(avail.size());
			for (const auto i : avail) {
				if (!std::binary_search(gone.begin(), gone.end(), i)) {
					rest.push_back(i);
				}
			}
			families.push_back(std::move(best));
			avail = std::move(rest);
		}
		leftover = std::move(avail);
		return families;
	}

	// Pieces that share no supported name still conflict over slots; saying so beats
	// claiming they have nothing in common.
	inline void ReportLeftovers(std::vector<Group>& groups, const std::vector<Piece>& pieces, const IndexList& rest)
	{
		if (rest.size() < 2) {
			return;
		}
		bool collides = false;
		for (std::size_t i = 0; i < rest.size() && !collides; ++i) {
			for (std::size_t j = i + 1; j < rest.size(); ++j) {
				if ((BodySlots(pieces[rest[i]].mask) & BodySlots(pieces[rest[j]].mask)) != 0) {
					collides = true;
					break;
				}
			}
		}
		if (!collides) {
			return;
		}
		Group group;
		group.members = rest;
		group.issue = "overlapping-slot-masks";
		groups.push_back(std::move(group));
	}

	inline std::vector<Group> Plan(const std::vector<Piece>& pieces, const Aliases& aliases = {}, bool useLabels = true, Rand* rng = nullptr)
	{
		if (pieces.size() == 1 && !pieces.front().key.empty() && pieces.front().mask) {
			return { Group{ {}, { 0 }, { { 0 } }, nullptr } };
		}
		IndexList              plain;
		std::map<Key, Group>   labelled;
		for (std::size_t i = 0; i < pieces.size(); ++i) {
			const auto& piece = pieces[i];
			if (piece.key.empty() || !piece.mask || piece.key.size() > 256) {
				continue;
			}
			// A closed leading label is an explicit boundary, not an arbitrary
			// character-prefix cut. Never merge pieces across different labels.
			if (useLabels && piece.key.front() == U'[') {
				const auto end = piece.key.find_first_of(U"[]", 1);
				if (end > 1 && end < piece.key.size() - 1 && piece.key[end] == U']') {
					auto& group = labelled[piece.key.substr(0, end + 1)];
					group.members.push_back(i);
					continue;
				}
			}
			plain.push_back(i);
		}
		const auto       fullKeys = [&pieces](std::size_t i) -> std::u32string_view { return pieces[i].key; };
		std::vector<Group> groups;
		for (auto& [label, group] : labelled) {
			// The label names a style, not a compatible variant. If the text after
			// it splits into independently supported styles, keep them apart.
			std::vector<Piece> suffixes;
			suffixes.reserve(group.members.size());
			bool complementary = false;
			for (const auto i : group.members) {
				const auto mask = pieces[i].mask;
				complementary |= std::any_of(suffixes.begin(), suffixes.end(), [mask](const auto& part) {
					return (BodySlots(mask) & BodySlots(part.mask)) == 0;
				});
				suffixes.push_back({ pieces[i].key.substr(label.size()), mask });
			}
			auto deeper = Plan(suffixes, aliases, false, rng);
			if (deeper.size() > 1) {
				for (auto& child : deeper) {
					child.prefix = label + child.prefix;
					for (auto& i : child.members) {
						i = group.members[i];
					}
					for (auto& entry : child.pending) {
						entry.first = group.members[entry.first];
					}
					for (auto& variant : child.variants) {
						for (auto& i : variant) {
							i = group.members[i];
						}
					}
					groups.push_back(std::move(child));
				}
				continue;
			}
			group.prefix = label;
			if (!complementary && group.members.size() > 1) {
				group.issue = "no-complementary-slots";
			} else {
				MatchVariants(group, pieces, aliases, rng);
			}
			groups.push_back(std::move(group));
		}
		IndexList leftover;
		for (auto& family : FamiliesOf(pieces, aliases, plain, fullKeys, leftover, false, rng)) {
			groups.push_back(std::move(family));
		}
		if (!leftover.empty()) {
			IndexList rest;
			for (auto& family : FamiliesOf(pieces, aliases, leftover, fullKeys, rest, true, rng)) {
				groups.push_back(std::move(family));
			}
			ReportLeftovers(groups, pieces, rest);
		}
		return groups;
	}
}
