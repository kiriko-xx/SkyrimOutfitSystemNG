#pragma once

#include <algorithm>
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
	inline constexpr std::size_t kMaxVariants = 32;
	inline constexpr std::size_t kMaxOutfits = 96;
	using IndexList = std::vector<std::size_t>;
	using Key = std::u32string;
	using Aliases = std::map<Key, Key>;  // explicit, already normalized; never infer translations

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
	};

	struct Node
	{
		std::map<char32_t, std::size_t> children;
		IndexList members;
		std::uint32_t coverage{};
		bool complementary{};
	};

	inline bool FamilyEvidence(const Node& node, std::u32string_view prefix)
	{
		// Short shared ASCII fragments (e.g. Da in Dark/Dawn) are weak evidence.
		// Non-ASCII languages need no word tokenizer; one code point is still too weak.
		const bool ascii = std::all_of(prefix.begin(), prefix.end(), [](char32_t ch) { return ch < 128; });
		return node.complementary && prefix.size() >= (ascii ? 3u : 2u);
	}

	inline void FindFamilies(const std::vector<Node>& trie, std::size_t index, Key& prefix, std::vector<Group>& groups)
	{
		const auto& node = trie[index];
		const auto before = groups.size();
		for (const auto& [ch, child] : node.children) {
			if (!trie[child].complementary) {
				continue;
			}
			prefix.push_back(ch);
			FindFamilies(trie, child, prefix, groups);
			prefix.pop_back();
		}
		if (!FamilyEvidence(node, prefix)) {
			return;
		}
		if (groups.size() == before) {
			groups.push_back({ prefix, node.members, {}, nullptr, node.coverage });
			return;
		}
		if (groups.size() == before + 1) {
			const auto& deeper = groups.back();
			if (deeper.coverage != node.coverage) {
				// A deeper family covers fewer slots. The remaining pieces might be
				// shared accessories or a different style; never silently drop them
				// and write an apparently complete outfit.
				groups.back() = { prefix, node.members, {}, "uncertain-family-coverage", node.coverage };
			}
		}
		// Multiple independently supported branches stay separate. No orphan bucket.
	}

	inline void MatchVariants(Group& group, const std::vector<Piece>& pieces, const Aliases& aliases)
	{
		std::map<std::uint32_t, IndexList> buckets;
		std::uint32_t occupied = 0;
		for (const auto i : group.members) {
			const auto mask = pieces[i].mask;
			if (!buckets.contains(mask) && (occupied & mask) != 0) {
				group.issue = "overlapping-slot-masks";
				return;
			}
			occupied |= mask;
			buckets[mask].push_back(i);
		}

		IndexList shared;
		std::vector<std::map<Key, std::size_t>> choices;
		for (const auto& [mask, bucket] : buckets) {
			if (bucket.size() == 1) {
				shared.push_back(bucket.front());
				continue;
			}
			if (bucket.size() > kMaxVariants) {
				group.issue = "variant-limit";
				return;
			}
			const std::u32string_view first = pieces[bucket.front()].key;
			std::size_t common = first.size();
			for (const auto i : bucket) {
				const std::u32string_view other = pieces[i].key;
				common = static_cast<std::size_t>(std::mismatch(first.begin(), first.begin() + common,
					other.begin(), other.end()).first - first.begin());
			}
			auto& tags = choices.emplace_back();
			for (const auto i : bucket) {
				Key tag = pieces[i].key.substr(common);
				if (const auto alias = aliases.find(tag); alias != aliases.end() && !tag.empty()) {
					tag = alias->second;
				}
				if (tag.empty() || !tags.emplace(std::move(tag), i).second) {
					group.issue = "non-unique-difference";
					return;
				}
			}
		}
		if (choices.empty()) {
			std::sort(shared.begin(), shared.end());
			group.variants.push_back(std::move(shared));
			return;
		}
		// Equality of the complete key sets proves one unique selection per
		// bucket for every variant. No Cartesian search or partial writes.
		const auto& tags = choices.front();
		for (const auto& choice : choices) {
			if (choice.size() != tags.size() || !std::equal(tags.begin(), tags.end(), choice.begin(),
					[](const auto& lhs, const auto& rhs) { return lhs.first == rhs.first; })) {
				group.issue = "unmatched-difference";
				return;
			}
		}
		for (const auto& [tag, index] : tags) {
			auto variant = shared;
			for (const auto& choice : choices) {
				variant.push_back(choice.at(tag));
			}
			std::sort(variant.begin(), variant.end());
			group.variants.push_back(std::move(variant));
		}
	}

	inline std::vector<Group> Plan(const std::vector<Piece>& pieces, const Aliases& aliases = {}, bool useLabels = true)
	{
		if (pieces.size() == 1 && !pieces.front().key.empty() && pieces.front().mask) {
			return { Group{ {}, { 0 }, { { 0 } }, nullptr } };
		}
		std::vector<Node> trie(1);
		std::map<Key, Group> labelled;
		for (std::size_t i = 0; i < pieces.size(); ++i) {
			const auto& [key, mask] = pieces[i];
			if (key.empty() || !mask) {
				continue;
			}
			std::size_t node = 0;
			if (key.size() > 256) {
				continue;
			}
			// A closed leading label is an explicit boundary, not an arbitrary
			// character-prefix cut. Never merge pieces across different labels.
			if (useLabels && key.front() == U'[') {
				const auto end = key.find_first_of(U"[]", 1);
				if (end > 1 && end < key.size() - 1 && key[end] == U']') {
					auto& group = labelled[key.substr(0, end + 1)];
					group.members.push_back(i);
					group.coverage |= mask;
					continue;
				}
			}
			for (const auto ch : key) {
				auto [entry, inserted] = trie[node].children.try_emplace(ch, trie.size());
				node = entry->second;
				if (inserted) {
					trie.emplace_back();
				}
				auto& current = trie[node];
				current.coverage |= mask;
				if (!current.complementary) {
					current.complementary = std::any_of(current.members.begin(), current.members.end(), [&](auto j) {
						return (pieces[j].mask & mask) == 0;
					});
				}
				current.members.push_back(i);
			}
		}
		std::vector<Group> groups;
		Key prefix;
		FindFamilies(trie, 0, prefix, groups);
		for (auto& [label, group] : labelled) {
			// The label identifies a family, not a compatible variant. If the
			// full label is shared by several independently supported families,
			// preserve those subfamilies instead of gluing them together.
			std::vector<Piece> suffixes;
			suffixes.reserve(group.members.size());
			bool complementary = false;
			for (auto i : group.members) {
				const auto mask = pieces[i].mask;
				complementary |= std::any_of(suffixes.begin(), suffixes.end(), [mask](const auto& part) {
					return (mask & part.mask) == 0;
				});
				suffixes.push_back({ pieces[i].key.substr(label.size()), mask });
			}
			auto deeper = Plan(suffixes, aliases, false);
			if (deeper.size() > 1) {
				for (auto& child : deeper) {
					child.prefix = label + child.prefix;
					for (auto& i : child.members) {
						i = group.members[i];
					}
					child.variants.clear();  // match again using the original names
					groups.push_back(std::move(child));
				}
				continue;
			}
			group.prefix = label;
			if (!complementary && group.members.size() > 1) {
				group.issue = "no-complementary-slots";
			}
			groups.push_back(std::move(group));
		}
		for (auto& group : groups) {
			if (!group.issue) {
				MatchVariants(group, pieces, aliases);
			}
		}
		return groups;
	}
}
