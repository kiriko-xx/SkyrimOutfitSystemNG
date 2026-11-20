#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Small read-only helper shared by the two "generate from a plugin" features
// (outfits and golden items). It answers three questions against the current
// load order:
//   * which plugins are loaded, in load order,
//   * which armour records belong to a plugin,
//   * which of those records actually cover a display slot.
// Everything is cached after the first call: the form array and the file list
// only change when plugins are loaded, which happens before the menu is usable.

namespace RE
{
	class TESObjectARMO;
	class TESForm;
}

namespace SOS::PluginScan
{
	struct PluginEntry
	{
		std::string   name;   // file name as the game stores it, e.g. "Example.esp"
		std::uint32_t order;  // 0-based load order position, ascending
		bool          light{ false };
	};

	// One row of the "pick a plugin" list.
	struct PluginSummary
	{
		std::string   name;
		std::uint32_t order{ 0 };
		std::size_t   armors{ 0 };        // every slot-bearing armour record
		std::size_t   outfitArmors{ 0 };  // those that pass IsOutfitArmor
	};

	// Every loaded plugin, ascending load order (normal files first, then
	// light files). Includes plugins without armour.
	std::vector<PluginEntry> LoadedPlugins();

	// Plugins that contain at least one slot-bearing armour, ascending load
	// order. The menu shows them reversed so the last loaded file is on top.
	std::vector<PluginSummary> PluginsWithArmor();

	// File name (with extension) that owns the form, "" when unknown.
	std::string OwningPlugin(const RE::TESForm* a_form);

	// True when the record covers at least one of the 32 display slots.
	bool IsSlotArmor(const RE::TESObjectARMO* a_armor);

	// True when the record is ordinary wearable clothing or armour, which is
	// what "generate outfits from a plugin" is allowed to pick up. The FX slot
	// (index 31, creation kit slot 61) is a visual effect channel, not clothing,
	// so a record that only covers FX is rejected; a record without an editor id
	// is a patch stub or a placeholder; a record without any armour addon model
	// has nothing to draw. Golden items deliberately skip this test: they may
	// hold FX pieces and unnamed records.
	bool IsOutfitArmor(const RE::TESObjectARMO* a_armor);

	// Slot-bearing armour records of that plugin, in form array order. The name
	// is matched case-insensitively. An unknown plugin returns an empty list.
	std::vector<RE::TESObjectARMO*> ArmorsOf(std::string_view a_plugin);

	// Lookup of a plugin-local form id, e.g. ("Example.esp", 0x000800).
	// Returns nullptr when the plugin is not loaded or the id is not a slot
	// armour.
	RE::TESObjectARMO* ResolveArmor(std::string_view a_plugin, std::uint32_t a_localId);

	// Drops the cached view of the load order. Only needed after a plugin is
	// loaded at runtime; the normal load order never changes.
	void Invalidate();
}
