#pragma once

namespace SOS::SceneOutfits
{
	// Listens for the SexLab and OStim scene start/end mod events.
	bool Install();

	// Re-schedules pending scene events after a task ran off the main thread.
	void Tick();

	// Drops the saved scene state (new game, load, revert).
	void Reset();
}
