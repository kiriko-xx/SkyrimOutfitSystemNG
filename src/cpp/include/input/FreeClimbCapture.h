#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// ---------------------------------------------------------------------------
// The hotkey recorder of this plugin is FreeClimb's recorder.
//
// Source: https://github.com/Eelta/FreeClimb -- src/InputBindings.h,
// src/GamepadInput.h and src/BindingCapture.h at commit 8a48ffe. The logic of
// everything below is copied unchanged; only the pieces this plugin needs were
// taken over, and the differences are all additive and marked with "local:".
//
// What the copy does, in FreeClimb's own words: the recorder only ever looks at
// the snapshot of what is held right now, and it stores a chord once every key
// of it has been released again. A key that goes up before the next one goes
// down is refused as "not pressed together" instead of being stored as a
// one-key chord, and nothing is accepted before whatever was held when the
// recorder opened has been released - which is what keeps Alt out of an Alt+O
// recording and the click out of every recording.
//
// Not copied on purpose: FreeClimb's binding sets (movement keys, hop), its
// gameplay ownership filter and its settings menu. The ownership filter is what
// FreeClimb uses to take the recorded keys off the input queue; this plugin does
// not touch the input queue at all, it leaves the menu to the menu framework
// (see the note in NGHotkeys.cpp).
// ---------------------------------------------------------------------------

namespace fc
{
	// ---- src/InputBindings.h ----------------------------------------------

	using KeyCode = std::uint16_t;
	inline constexpr KeyCode anyShift = 0x100, anyControl = 0x101, anyAlt = 0x102;

	struct KeyChord
	{
		std::array<KeyCode, 4> keys{};
		std::uint8_t           count{};
		bool                   operator==(const KeyChord&) const = default;
	};

	struct KeyName
	{
		std::string_view name;
		KeyCode          code;
	};

	inline constexpr KeyName keyNames[] = {
		{ "Shift", anyShift }, { "Ctrl", anyControl }, { "Alt", anyAlt },
		{ "LShift", 0x2a }, { "RShift", 0x36 }, { "LCtrl", 0x1d }, { "RCtrl", 0x9d }, { "LAlt", 0x38 }, { "RAlt", 0xb8 },
		{ "A", 0x1e }, { "B", 0x30 }, { "C", 0x2e }, { "D", 0x20 }, { "E", 0x12 }, { "F", 0x21 }, { "G", 0x22 },
		{ "H", 0x23 }, { "I", 0x17 }, { "J", 0x24 }, { "K", 0x25 }, { "L", 0x26 }, { "M", 0x32 }, { "N", 0x31 },
		{ "O", 0x18 }, { "P", 0x19 }, { "Q", 0x10 }, { "R", 0x13 }, { "S", 0x1f }, { "T", 0x14 }, { "U", 0x16 },
		{ "V", 0x2f }, { "W", 0x11 }, { "X", 0x2d }, { "Y", 0x15 }, { "Z", 0x2c },
		{ "0", 0x0b }, { "1", 0x02 }, { "2", 0x03 }, { "3", 0x04 }, { "4", 0x05 }, { "5", 0x06 }, { "6", 0x07 }, { "7", 0x08 }, { "8", 0x09 }, { "9", 0x0a },
		{ "Space", 0x39 }, { "Tab", 0x0f }, { "Enter", 0x1c }, { "Backspace", 0x0e }, { "CapsLock", 0x3a },
		{ "Minus", 0x0c }, { "Equals", 0x0d }, { "LeftBracket", 0x1a }, { "RightBracket", 0x1b },
		{ "Semicolon", 0x27 }, { "Apostrophe", 0x28 }, { "Backslash", 0x2b }, { "Comma", 0x33 }, { "Period", 0x34 }, { "Slash", 0x35 },
		{ "Up", 0xc8 }, { "Down", 0xd0 }, { "Left", 0xcb }, { "Right", 0xcd }, { "Home", 0xc7 }, { "End", 0xcf },
		{ "PageUp", 0xc9 }, { "PageDown", 0xd1 }, { "Insert", 0xd2 }, { "Delete", 0xd3 },
		{ "F1", 0x3b }, { "F2", 0x3c }, { "F3", 0x3d }, { "F4", 0x3e }, { "F5", 0x3f }, { "F6", 0x40 },
		{ "F7", 0x41 }, { "F8", 0x42 }, { "F9", 0x43 }, { "F10", 0x44 }, { "F11", 0x57 }, { "F12", 0x58 },
		{ "Num0", 0x52 }, { "Num1", 0x4f }, { "Num2", 0x50 }, { "Num3", 0x51 }, { "Num4", 0x4b },
		{ "Num5", 0x4c }, { "Num6", 0x4d }, { "Num7", 0x47 }, { "Num8", 0x48 }, { "Num9", 0x49 },
		{ "NumEnter", 0x9c }, { "NumPlus", 0x4e }, { "NumMinus", 0x4a }, { "NumMultiply", 0x37 }, { "NumDivide", 0xb5 }, { "NumDecimal", 0x53 },
		// local: the three keys this plugin's ini can already name, so an existing
		// "grave" / "numlock" / "scrolllock" binding stays recordable.
		{ "Grave", 0x29 }, { "NumLock", 0x45 }, { "ScrollLock", 0x46 }
	};

	inline KeyCode keyGroup(KeyCode key)
	{
		if (key == 0x2a || key == 0x36 || key == anyShift) return anyShift;
		if (key == 0x1d || key == 0x9d || key == anyControl) return anyControl;
		if (key == 0x38 || key == 0xb8 || key == anyAlt) return anyAlt;
		return key;
	}

	inline bool keyImplies(KeyCode a, KeyCode b) { return a == b || (b >= anyShift && keyGroup(a) == b); }
	inline bool keyOverlaps(KeyCode a, KeyCode b) { return keyImplies(a, b) || keyImplies(b, a); }

	inline bool validKeyCode(KeyCode key)
	{
		for (const auto& named : keyNames)
			if (named.code == key) return true;
		return false;
	}

	inline bool chordHasGroup(const KeyChord& chord, KeyCode key)
	{
		for (unsigned i = 0; i < chord.count && i < chord.keys.size(); ++i)
			if (keyGroup(chord.keys[i]) == key) return true;
		return false;
	}

	inline bool validKeyChord(const KeyChord& chord)
	{
		if (!chord.count || chord.count > chord.keys.size()) return false;
		for (unsigned i = 0; i < chord.count; ++i) {
			if (!validKeyCode(chord.keys[i])) return false;
			for (unsigned j = 0; j < i; ++j)
				if (keyOverlaps(chord.keys[i], chord.keys[j])) return false;
		}
		if (chordHasGroup(chord, anyAlt) && (chordHasGroup(chord, 0x0f) || chordHasGroup(chord, 0x3e))) return false;
		if (chordHasGroup(chord, anyAlt) && chordHasGroup(chord, anyControl) && chordHasGroup(chord, 0xd3)) return false;
		return true;
	}

	inline std::string serializeKeyChord(const KeyChord& chord)
	{
		if (!validKeyChord(chord)) return {};
		std::string result;
		for (unsigned i = 0; i < chord.count; ++i)
			for (const auto& named : keyNames)
				if (named.code == chord.keys[i]) {
					if (!result.empty()) result += '+';
					result += named.name;
					break;
				}
		return result;
	}

	struct InputState
	{
		std::array<bool, 256> down{};
		void                  set(unsigned scan, bool pressed)
		{
			if (scan < down.size()) down[scan] = pressed;
		}
		void reset() { down.fill(false); }
		bool held(KeyCode key) const
		{
			if (key == anyShift) return down[0x2a] || down[0x36];
			if (key == anyControl) return down[0x1d] || down[0x9d];
			if (key == anyAlt) return down[0x38] || down[0xb8];
			return key < down.size() && down[key];
		}
	};

	// ---- src/GamepadInput.h -----------------------------------------------

	using GamepadChord = std::uint16_t;

	struct GamepadButtonName
	{
		std::string_view name;
		unsigned         index;
	};

	inline constexpr std::array<GamepadButtonName, 16> gamepadButtonNames{ {
		{ "DPadUp", 0 }, { "DPadDown", 1 }, { "DPadLeft", 2 }, { "DPadRight", 3 },
		{ "Start", 4 }, { "Back", 5 }, { "LS", 6 }, { "RS", 7 }, { "LB", 8 }, { "RB", 9 },
		{ "A", 10 }, { "B", 11 }, { "X", 12 }, { "Y", 13 }, { "LT", 14 }, { "RT", 15 }
	} };

	inline constexpr GamepadChord gamepadButtonMask(unsigned index)
	{
		return index < 16 ? static_cast<GamepadChord>(1u << index) : GamepadChord{};
	}

	inline bool validGamepadChord(GamepadChord chord)
	{
		return chord && !(chord & (gamepadButtonMask(4) | gamepadButtonMask(5))) && std::popcount(chord) <= 4;
	}

	inline std::string serializeGamepadChord(GamepadChord chord)
	{
		if (!validGamepadChord(chord)) return {};
		std::string result;
		for (const auto& named : gamepadButtonNames)
			if (chord & gamepadButtonMask(named.index)) {
				if (!result.empty()) result += '+';
				result += named.name;
			}
		return result;
	}

	// ---- src/BindingCapture.h ---------------------------------------------

	enum class BindingCaptureDevice
	{
		keyboard,
		gamepad
	};

	enum class BindingCaptureStatus
	{
		idle,
		waiting,
		armed,
		captured,
		error,
		cancelled
	};

	enum class BindingCaptureError
	{
		none,
		invalid,
		tooMany,
		notSimultaneous
	};

	struct BindingCaptureSnapshot
	{
		InputState   keyboard{};
		GamepadChord gamepad{};
		bool         keyboardValid{}, gamepadValid{}, activationHeld{};
	};

	struct BindingCaptureResult
	{
		BindingCaptureStatus status = BindingCaptureStatus::idle;
		BindingCaptureDevice device = BindingCaptureDevice::keyboard;
		BindingCaptureError  error = BindingCaptureError::none;
		KeyChord             keyboard{};
		GamepadChord         gamepad{};
		std::string          preview;
	};

	class BindingCapture
	{
		BindingCaptureResult value;
		std::bitset<256>     candidate;

		const BindingCaptureResult& finish(BindingCaptureStatus status, BindingCaptureError error = BindingCaptureError::none)
		{
			const auto device = value.device;
			value = {};
			value.device = device;
			value.status = status;
			value.error = error;
			candidate.reset();
			return value;
		}

	public:
		const BindingCaptureResult& begin(BindingCaptureDevice device)
		{
			value = {};
			value.device = device;
			value.status = BindingCaptureStatus::waiting;
			candidate.reset();
			return value;
		}

		const BindingCaptureResult& sample(const BindingCaptureSnapshot& snapshot)
		{
			if (!active()) return value;
			if (snapshot.keyboardValid && snapshot.keyboard.held(0x01)) return cancel();
			const bool keyboard = value.device == BindingCaptureDevice::keyboard;
			if (snapshot.gamepadValid && (snapshot.gamepad & gamepadButtonMask(5))) return cancel();
			if (keyboard ? !snapshot.keyboardValid : !snapshot.gamepadValid) return value;
			std::bitset<256> pressed;
			if (keyboard) {
				for (std::size_t i = 0; i < snapshot.keyboard.down.size(); ++i) pressed[i] = snapshot.keyboard.down[i];
			} else {
				for (unsigned i = 0; i < 16; ++i) pressed[i] = (snapshot.gamepad & gamepadButtonMask(i)) != 0;
			}
			if (value.status == BindingCaptureStatus::waiting) {
				if (pressed.none() && !snapshot.activationHeld) value.status = BindingCaptureStatus::armed;
				return value;
			}
			if (pressed.none()) {
				if (candidate.any()) value.status = BindingCaptureStatus::captured;
				return value;
			}
			if (pressed.count() > 4) return finish(BindingCaptureStatus::error, BindingCaptureError::tooMany);
			KeyChord chord;
			if (keyboard) {
				for (unsigned i = 0; i < 256; ++i)
					if (pressed[i]) chord.keys[chord.count++] = static_cast<KeyCode>(i);
				if (!validKeyChord(chord)) return finish(BindingCaptureStatus::error, BindingCaptureError::invalid);
			} else if (!validGamepadChord(snapshot.gamepad))
				return finish(BindingCaptureStatus::error, BindingCaptureError::invalid);
			if ((pressed & ~candidate).none()) return value;
			if ((pressed & candidate) != candidate) return finish(BindingCaptureStatus::error, BindingCaptureError::notSimultaneous);
			candidate = pressed;
			if (keyboard) {
				std::sort(chord.keys.begin(), chord.keys.begin() + chord.count, [](KeyCode a, KeyCode b) {
					const bool am = keyGroup(a) >= anyShift, bm = keyGroup(b) >= anyShift;
					return am != bm ? am : a < b;
				});
				value.keyboard = chord;
				value.preview = serializeKeyChord(chord);
			} else {
				value.gamepad = snapshot.gamepad;
				value.preview = serializeGamepadChord(snapshot.gamepad);
			}
			return value;
		}

		const BindingCaptureResult& cancel() { return finish(BindingCaptureStatus::cancelled); }
		const BindingCaptureResult& result() const { return value; }
		bool                        active() const { return value.status == BindingCaptureStatus::waiting || value.status == BindingCaptureStatus::armed; }
		void                        reset()
		{
			value = {};
			candidate.reset();
		}
	};

}
