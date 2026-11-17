#pragma once

#include <string>

namespace RE
{
	class Actor;
}

// Entry point of the per-mod compatibility layer (replaces the former generic
// "perception" hooks, which no mod in the wild actually called).
namespace SOS::Compat
{
	struct Status
	{
		bool andDetected{ false };
		bool andHooked{ false };
		std::string andVersion;
		bool padDetected{ false };
		std::string padVersion;
	};

	// Called once at kDataLoaded: patches the known mod entry points.
	bool Install();

	// Re-reads the toggles/status (after a load, or when the menu changes a
	// setting). Never patches twice.
	void Reload();

	const Status& GetStatus();

	// Called after an outfit was pushed to an actor: re-asserts the per-mod state.
	void OnOutfitApplied(RE::Actor* a_actor);

	// Cheap drift correction (throttled internally): AND rescans the real
	// equipment on its own schedule and would reset the ranks we wrote.
	void Tick();
}
