#pragma once

#include <string>

namespace RE
{
	class Actor;
}

// Advanced Nudity Detection (AND) compatibility.
//
// AND does not offer a script or plugin API for "what the actor looks like", but
// it publishes its verdict through ordinary factions: AND_NudeActorFaction,
// AND_ToplessFaction, AND_BottomlessFaction (plus the Showing* ones). Its own
// Papyrus getters (AND_ModEventListener.GetNude/GetTopless/GetBottomless) and
// every dialogue condition / arousal mod read exactly those ranks, so writing
// the ranks is the documented, data level contract - no code patching of the
// other mod, and it keeps working when AND is updated as long as the faction
// EditorIDs stay the same (they are referenced by its dialogue conditions).
//
// AND rescans the real equipment now and then (equip events, game time ticks)
// and would overwrite our ranks, so we simply re-assert them after every outfit
// refresh and once per second while the player is active.
namespace SOS::Compat::And
{
	bool Detected();
	std::string Version();
	bool Hooked();  // kept for the status UI: true once the factions were found
	const char* StatusText();

	bool Install();
	void Reload();

	// Writes the nudity ranks that match the outfit that is currently shown.
	void Sync(RE::Actor* a_actor);
}
