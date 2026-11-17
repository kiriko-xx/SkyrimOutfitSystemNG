#pragma once

namespace SOS::Menu
{
	// Registers the settings/browser menu with the SKSE Menu Framework.
	// Returns false when the framework is not installed; the plugin keeps working.
	bool Register();

	// True once the section items were registered.
	bool Registered();

	// Drops the cached outfit lists so that the next frame shows the current
	// store (the editor calls this after every change).
	void InvalidateBrowser();

	// True while one of our ImGui widgets owns the keyboard.
	bool TextInputActive();

	// ---- quick picker -------------------------------------------------------
	// A small standalone window (search, paging, one click to wear) so that the
	// "Change Outfit" lesser power can pop it up without going through the menu.
	bool QuickPickerVisible();
	void ShowQuickPicker(bool a_show);
	void ToggleQuickPicker();
}
