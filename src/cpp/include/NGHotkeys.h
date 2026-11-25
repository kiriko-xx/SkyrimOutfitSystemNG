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
