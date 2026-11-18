#pragma once

namespace SOS::SceneOutfits
{
	// Listens for the SexLab and OStim scene start/end mod events.
	bool Install();

	// Drops the saved scene state (new game, load, revert).
	void Reset();
}
