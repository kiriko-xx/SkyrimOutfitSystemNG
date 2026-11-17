#pragma once

#include <cstdint>
#include <vector>

#include "core/Types.h"

namespace RE
{
	class Actor;
	class BGSKeyword;
	class TESBoundObject;
	class TESObjectARMO;
}

// Shared helpers for the per-mod compatibility layer.
//
// The outfit display set is the single source of truth for "what the player can
// actually see": for every biped slot, the outfit item that wins it, or nothing
// when the outfit hides the slot. The compatible mods are answered from this set
// so that an outfit piece that covers nothing counts as bare skin everywhere.
namespace SOS::Compat
{
	std::uint32_t SlotMaskOf(const RE::TESObjectARMO* a_armor);

	// Every armour the actor really wears (worn flags in the inventory).
	bool CollectWornArmors(RE::Actor* a_actor, std::vector<const RE::TESObjectARMO*>& a_out);

	// True while an outfit drives the actor's appearance and the world should be
	// told about it: the plugin is enabled, an outfit is assigned, no save is
	// being loaded and the actor actually has a biped.
	bool OverrideActive(RE::Actor* a_actor);

	// The armour pieces the player can see, one winner per slot.
	bool ComputeDisplaySet(RE::Actor* a_actor, std::vector<TESObjectARMOPtr>& a_out);

	// Does the visible set carry the keyword? (false when the outfit hides the
	// piece, so "empty outfit piece" reads as bare skin.)
	bool DisplayHasKeyword(RE::Actor* a_actor, const RE::BGSKeyword* a_keyword);

	// Vanilla answer: does any real worn item carry the keyword?
	bool VanillaWornHasKeyword(RE::Actor* a_actor, const RE::BGSKeyword* a_keyword);

	// Vanilla answer: every worn object the game would report (keyword forms only,
	// which is what Advanced Nudity Detection collects).
	void VanillaWornObjects(RE::Actor* a_actor, std::vector<RE::TESBoundObject*>& a_out);
}
