#pragma once

#include <string>

namespace RE
{
	class Actor;
}

// Procedural Armor Damage (PAD) compatibility.
//
// PAD damages armour by measuring the triangles that are actually drawn: it walks
// the player's biped, keeps the slots whose item is a real worn armour, and then
// captures the mesh found there. Skyrim Outfit System's skinning pass restores
// biped.objects[i].item to the real armour after it applies the outfit addon, so
// PAD sees the real armour and captures the *outfit* mesh that is drawn over it.
// Holes therefore appear on the armour the player can see while the damage is
// stored on the real armour - which is exactly the behaviour we want.
//
// A piece the outfit shows over a slot with no real armour cannot be damaged:
// there is no armour item for PAD to store the damage on. The diagnostics below
// report those pieces so users know why nothing tears there.
namespace SOS::Compat::Pad
{
	bool Detected();
	std::string Version();

	// One-time overview written to the log after kDataLoaded.
	void ReportStatus();

	// Called after an outfit was pushed to an actor. Logs the slots whose visible
	// piece has no real armour underneath (once per distinct set); per-piece
	// details only with ExtraLogging.
	void OnOutfitApplied(RE::Actor* a_actor);

	// Writes a full per-slot table to the log: what the outfit shows, what the
	// actor really wears, and whether PAD can reach that slot at all. Bound to
	// the "Log slot report" button so a user can produce evidence in one click.
	void Diagnose(RE::Actor* a_actor);

	const char* StatusText();
}
