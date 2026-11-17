#pragma once

namespace SOS::GameState
{
	// True while the game is reverting to the menu or loading a save. During
	// that window actors exist but their biped data / 3D is still being built,
	// so the skinning hooks and the GetWornForm override must stay out of the
	// way: touching an actor there is what turned the load screen into a hang.
	[[nodiscard]] bool IsLoading();
	void SetLoading(bool a_loading);
}// namespace SOS::GameState
