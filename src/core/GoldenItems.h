#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Golden items ("金件") are the armour pieces the user wants to keep on top of
// an outfit no matter which outfit is worn. They used to live as a flat
// plugin:localid list in SkyrimOutfitSystemNG.ini; they now have their own
// store, grouped by the plugin that defines them, because the menu shows one
// row per plugin.
//
// The store is read by the skinning hooks while they decide what to draw, so
// the lookups go through an immutable snapshot: rebuilding it never blocks or
// tears a hook that is already running.

namespace RE
{
	class TESObjectARMO;
}

namespace SOS::Golden
{
	struct Group
	{
		std::string              plugin;     // file name with extension
		std::vector<std::uint32_t> localIds; // local form ids inside that plugin, ascending
	};

	// Path of the standalone JSON file (separate from the outfit JSON).
	std::string JsonPath();

	// ---- reading ----------------------------------------------------------
	// Resolves the plugin-local ids into full form ids. Safe to call often; it
	// only rebuilds when the content changed.
	void WarmUp();
	// True when the armour is a golden item.
	bool IsGolden(const RE::TESObjectARMO* a_armor);

	std::vector<Group> Groups();  // sorted by plugin name, case-insensitive
	std::size_t        GroupCount();
	std::size_t        TotalCount();

	// Writes the entries of the legacy ini list into the store, but only the
	// ones that are not there yet. Returns how many were added.
	std::size_t ApplyLegacyEntries(const std::vector<std::pair<std::string, std::uint32_t>>& a_entries);

	// ---- editing ----------------------------------------------------------
	// Adds a resolved armour record; the plugin and the local id are derived
	// from the form itself.
	bool AddArmor(const RE::TESObjectARMO* a_armor);
	// Adds "plugin:localId" after checking that the record really exists.
	bool AddLocal(const std::string& a_plugin, std::uint32_t a_localId, std::string& a_message);
	// Adds every slot armour of a plugin; returns how many were added.
	std::size_t AddAllFromPlugin(const std::string& a_plugin);
	bool        RemoveLocal(const std::string& a_plugin, std::uint32_t a_localId);
	bool        RemoveGroup(const std::string& a_plugin);

	// ---- persistence ------------------------------------------------------
	bool        LoadFromDisk();  // called once after the data is loaded
	bool        SaveToDisk();
	std::string ExportText();
	bool        ImportText(const std::string& a_json, std::string& a_message);
}
