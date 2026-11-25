#pragma once

#include <cstddef>
#include <string>

namespace SOS::Hotkeys
{
	// Installs the input sink. Every hotkey is a chord of 1 to 4 keyboard keys or
	// of 1 to 4 controller buttons, parsed from the ini strings (for example
	// "ctrl+shift+O" or "pad:lb+pad:rb"); an empty or unparsable value disables
	// that binding. A hotkey can be bound to the keyboard and to the controller at
	// the same time and both bindings fire.
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

	// Which device a recording listens to. The settings page has one button per
	// device, so a controller button can never be mixed into a keyboard chord and
	// a keyboard key can never end up in a controller chord.
	enum class Input
	{
		kKeyboard,
		kGamepad
	};

	void BeginCapture(Slot a_slot, Input a_device = Input::kKeyboard);

	// The device the running recording listens to. Only meaningful while
	// CaptureActive() is true.
	Input CaptureDevice();
	void CancelCapture();
	bool CaptureActive();

	// Human readable text of what has been pressed so far ("ctrl+shift+O").
	std::string CaptureText();

	// True once every key that was held when the recording started has been
	// released: only then is anything accepted, so the click that opened the
	// recorder cannot end up in the chord.
	bool CaptureArmed();

	// 0 = none, 1 = too many keys at once, 2 = that combination cannot be used,
	// 3 = the keys were not held together. Non-zero keeps the window open and
	// tells the user what to change.
	int CaptureProblem();

	// Moves a finished capture into a_slot / a_device / a_text and ends it.
	bool TakeCaptureResult(Slot& a_slot, Input& a_device, std::string& a_text);
}
