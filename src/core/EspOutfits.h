#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// "Generate outfits from a plugin": one click turns the armour records of a
// chosen plugin into switchable outfits instead of one unusable pile.
//
// 1.5.46 follows the "prefix tree + longest common prefix + slot constraint"
// design:
//   1. every name gets a comparison copy (code points in order, ASCII lower
//      case, separators dropped, a bracketed pack label removed); the original
//      spelling is what the outfit is named after, so no separator has to be
//      assumed and no character is ever cut in half;
//   2. costume detection walks a character level prefix tree. A branch is one
//      costume while the pieces under it do not each cover several slots of
//      their own; two costumes that only share a short prefix split, a costume
//      whose pieces differ on the last characters does not. Records that cannot
//      form a costume (single slot, single record) keep the plain plugin name;
//   3. inside one costume the pieces are bucketed by slot;
//   4. inside one bucket the part all candidates share is removed, and what is
//      left is the differential identifier ("A", "B", "red");
//   5. presets are generated per identifier: a slot with a single candidate is
//      shared by every preset, a slot with several has to hold the piece that
//      carries the current identifier. When the identifiers of two slots do not
//      correspond ("top A/B" next to "skirt red/black") nothing is generated for
//      that costume, because a guessed combination would look complete and be
//      wrong. Every preset is checked for slot uniqueness before it is written.
//
// Slot policies are always set to "require equipped" (XXXO): a generated
// outfit only draws on a slot the actor actually wears something on, which is
// what the old hand written scene outfits relied on.
namespace SOS::EspOutfits
{
	struct PluginSummary
	{
		std::string   name;
		std::uint32_t order{ 0 };
		std::size_t   armors{ 0 };        // every slot-bearing armour record
		std::size_t   outfitArmors{ 0 };  // the subset that counts as clothing
	};

	struct Result
	{
		bool                     ok{ false };
		std::string              message;  // English; the menu shows it verbatim
		std::vector<std::string> names;    // generated outfit names, first one first
		std::size_t              pieces{ 0 };
		std::size_t              created{ 0 };
		std::size_t              overwritten{ 0 };
	};

	// Every loaded plugin that holds at least one piece of ordinary clothing,
	// ascending load order. The menu shows this reversed so the last loaded file
	// is on top. FX effects, unnamed records and records without a model are
	// counted in "armors" but not in "outfitArmors", and a plugin whose records
	// are all like that is left out.
	std::vector<PluginSummary> PluginsWithArmor();

	// "Example.esp" -> "Example" (the extension is dropped; the outfit name is
	// what the user sees in the list).
	std::string BaseNameFor(const std::string& a_plugin);

	// "Example", "Example_1", "Example_2" ...
	std::string OutfitNameFor(const std::string& a_base, std::size_t a_index);

	// Builds the outfits. An existing outfit with a generated name is
	// overwritten in place so a re-run does not pile up duplicates.
	Result GenerateFromPlugin(const std::string& a_plugin, bool a_favorite);
}
