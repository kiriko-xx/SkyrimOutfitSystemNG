#pragma once

#include <cstddef>
#include <string>

namespace SOS::Hotkeys
{
	// Installs the input sink. Every hotkey is a chord of 1 to 4 keyboard keys
	// and/or controller buttons, parsed from the ini strings (for example
	// "ctrl+shift+O" or "pad:LB+pad:RB"); an empty or unparsable value disables
	// that hotkey.
	bool Install();
	void Reload();
	// Called from the render thread every frame. Polls the keyboard and the
	// pad so the settings page can record keys even when the game sends no
	// input events while its menu is open.
	void Tick();

	// The "Change Outfit" lesser power is cast with the game's own Shout/Power
	// key, so the plugin mirrors that binding instead of asking for a second key.
	std::string GamePowerKeyText();

	// True right after the key handler already did what the power would do, so the
	// effect script that runs after the cast animation can be ignored.
	bool PowerActionJustHandled();
	void SetTextInputActive(bool a_active);

	// ---- hotkey capture (settings page) ---------------------------------
	//
	// The settings page shows a button per hotkey. Clicking it starts a capture:
	// the sink records what the user presses (keyboard keys and controller
	// buttons, up to kMaxChordKeys) and reports the finished combination once
	// every key has been released again, so the settings code can write it to
	// the ini and reload the chords.
	enum class Slot
	{
		kRandomOutfit,
		kQuickPicker,
		kQuickPickerKey
	};

	inline constexpr std::size_t kMaxChordKeys = 4;

	void BeginCapture(Slot a_slot);
	void CancelCapture();
	bool CaptureActive();

	// Human readable text of what has been pressed so far ("ctrl+shift+O").
	std::string CaptureText();

	// True when the user pressed more keys than kMaxChordKeys while capturing.
	bool CaptureOverflowed();

	// Moves a finished capture into a_slot / a_text and ends the capture.
	bool TakeCaptureResult(Slot& a_slot, std::string& a_text);
}
