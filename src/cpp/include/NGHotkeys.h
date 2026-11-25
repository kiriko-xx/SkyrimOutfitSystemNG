#pragma once

#include <cstddef>
#include <string>

namespace RE
{
	class InputEvent;
}

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
	// Called when the framework opens or closes its menu. While the menu is open the
	// hotkeys stay quiet: the keys belong to the menu, and a key-up that the menu ate
	// must not leave a chord latched for the next frame.
	void NotifyMenuState(bool a_open);

	// The "Change Outfit" lesser power is cast with the game's own Shout/Power
	// key, so the plugin mirrors that binding instead of asking for a second key.
	std::string GamePowerKeyText();

	// True right after the key handler already did what the power would do, so the
	// effect script that runs after the cast animation can be ignored.
	bool PowerActionJustHandled();
	void SetTextInputActive(bool a_active);

	// ---- taking the recorded keys off the input queue --------------------
	//
	// The recorder's window has to own the keyboard and the pad completely: Escape,
	// Back and the shoulder buttons must end the recording instead of acting on the
	// menu framework underneath it. A sink cannot do that (the queue is dispatched to
	// every sink), so the two device polls are hooked and the events are unlinked from
	// the queue before anything is dispatched - this is FreeClimb's mechanism, using
	// the vtable constants CommonLibSSE-NG publishes. Both hooks are best effort: a
	// slot that cannot be hooked only logs a warning.
	void InstallInputFilters();
	// Forgets which keys an earlier recording took off the queue.
	void ResetInputOwnership();

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

	// Feeds raw events to a running recording. The input sink calls this itself
	// while the game is playing; the settings page calls it with the events the
	// framework hands to its input callback, because the game stops dispatching
	// button events to plugins once the framework menu is open. Nothing happens
	// when no recording runs.
	void FeedInputEvent(RE::InputEvent* a_event);

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
