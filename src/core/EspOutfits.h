#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// "Generate outfits from a plugin": one click turns the armour records of a
// chosen plugin into switchable outfits instead of one unusable pile.
//
// 1.5.45 groups the records by the words their names start with, because one
// plugin often ships several costumes ("Sailor Top"/"Sailor Skirt" is one
// costume, "School Top" is another). Inside one costume the pieces are split by
// slot conflicts: two alternatives for the same slot become <name> and
// <name>_1, and the second one keeps the base pieces on the slots it does not
// cover. Records whose names share nothing with the rest form one "rest"
// costume that keeps the plain plugin name.
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
