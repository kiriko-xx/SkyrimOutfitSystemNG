#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace RE
{
	class Actor;
}

namespace SOS::Features
{
	// Re-runs the equipment update for one actor so that a changed outfit becomes
	// visible immediately (same two calls the Papyrus RefreshArmorFor uses).
	void RefreshActor(RE::Actor* a_actor);

	// Outfit names, optionally restricted to favourites, sorted case-insensitively.
	std::vector<std::string> OutfitNames(bool a_favoritesOnly);

	// Every armour the actor is currently wearing (used by the menu browser).
	std::vector<const RE::TESObjectARMO*> WornArmors(RE::Actor* a_actor);

	// Every armour in the actor's inventory, worn or not (the old MCM's
	// "add to outfit, from inventory" list).
	std::vector<const RE::TESObjectARMO*> CarriedArmors(RE::Actor* a_actor);

	// Actors in the player's current cell (the old MCM's "add new..." list).
	std::vector<RE::Actor*> ActorsNearPlayer();

	// Adds or removes the "Change Outfit" lesser power so that the quickslot
	// switch of the options page keeps working without the Papyrus manager.
	void ApplyQuickslotSpell();

	// Creates (or overwrites) a named outfit with the actor's worn armour.
	bool CreateOutfitFromWorn(RE::Actor* a_actor, const char* a_name);

	// Picks a random outfit, assigns it to the actor and refreshes the appearance.
	// Returns the chosen name, or an empty string when there was nothing to pick.
	std::string PickRandomOutfit(RE::Actor* a_actor, bool a_favoritesOnly);

	// Assigns a named outfit (empty name clears the assignment) and refreshes.
	bool AssignOutfit(RE::Actor* a_actor, const char* a_name);

	// Re-evaluates the actor's current location/state and applies the outfit
	// configured for it. Used by the menu after a state entry is edited.
	bool ApplyCurrentStateOutfit(RE::Actor* a_actor);

	// Data/SKSE/Plugins/OutfitSystemData.json - the same file the original mod
	// reads and writes, so outfits can be moved between the old and the new
	// build. The message is already localised.
	std::string OutfitDataJsonPath();
	bool ExportOutfitsToJson(std::string& a_message);
	bool ImportOutfitsFromJson(std::string& a_message);

	// Papyrus wrappers (SkyrimOutfitSystemNativeFuncs).
	std::string RandomOutfit(RE::BSScript::IVirtualMachine* a_registry, std::uint32_t a_stackId, RE::StaticFunctionTag*, RE::Actor* a_actor, bool a_favoritesOnly);
	std::int32_t GetOutfitCount(RE::BSScript::IVirtualMachine* a_registry, std::uint32_t a_stackId, RE::StaticFunctionTag*);
	std::string GetOutfitNameByIndex(RE::BSScript::IVirtualMachine* a_registry, std::uint32_t a_stackId, RE::StaticFunctionTag*, std::int32_t a_index, bool a_favoritesOnly);
	std::vector<std::string> SearchOutfits(RE::BSScript::IVirtualMachine* a_registry, std::uint32_t a_stackId, RE::StaticFunctionTag*, std::string a_query, bool a_favoritesOnly);
	// Quick picker window (the "Change Outfit" lesser power uses these).
	bool OpenQuickPicker(RE::BSScript::IVirtualMachine* a_registry, std::uint32_t a_stackId, RE::StaticFunctionTag*);
	void CloseQuickPicker(RE::BSScript::IVirtualMachine* a_registry, std::uint32_t a_stackId, RE::StaticFunctionTag*);
	bool IsQuickPickerVisible(RE::BSScript::IVirtualMachine* a_registry, std::uint32_t a_stackId, RE::StaticFunctionTag*);

	// [RandomOutfit] QuickSlotRandom: when true the "Change Outfit" power rolls a
	// random outfit instead of opening the quick picker window.
	bool IsQuickSlotRandom(RE::BSScript::IVirtualMachine* a_registry, std::uint32_t a_stackId, RE::StaticFunctionTag*);
	void SetQuickSlotRandom(RE::BSScript::IVirtualMachine* a_registry, std::uint32_t a_stackId, RE::StaticFunctionTag*, bool a_enabled);
}
