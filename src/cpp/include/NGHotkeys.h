#pragma once

namespace SOS::Hotkeys
{
	// Installs the keyboard sink. Keys are parsed from Settings::randomOutfitHotkey
	// (e.g. "ctrl+shift+O"); an empty or invalid value disables the hotkey.
	bool Install();
	void Reload();
	void SetTextInputActive(bool a_active);
}
