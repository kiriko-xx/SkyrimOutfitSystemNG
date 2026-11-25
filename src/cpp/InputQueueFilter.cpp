#include "NGHotkeys.h"

#include <RE/Skyrim.h>
#include <REL/Relocation.h>
#include <SKSE/InputMap.h>
#include <Windows.h>

#include <bitset>
#include <cstddef>
#include <cstdint>

// ---------------------------------------------------------------------------
// The recorder takes the player's keys off the input queue, exactly the way
// FreeClimb does it.
//
// Source: https://github.com/Eelta/FreeClimb at commit 8a48ffe --
// src/SettingsMenu.cpp (the ownership filter and the queue walk) and
// src/Plugin.cpp (the two device hooks that call the filter). FreeClimb's own
// words for why this is needed: a key that reaches the input event queue reaches
// every sink of the game and every framework menu on top of it, so cancelling it
// from a plugin's own sink is too late. The events have to be taken off the queue
// before they are dispatched, and the only place that runs before the dispatch is
// the poll of the input device that produced them.
//
// The two vtable slots are addressed through the constants CommonLibSSE-NG
// publishes (RE::VTABLE_BSWin32KeyboardDevice / RE::VTABLE_BSPCGamepadDeviceHandler),
// so the version dependent address comes from Address Library like every other
// relocation in this plugin - the same two constants FreeClimb uses.
//
// The filter is only ever active while a recording runs: with no recording and
// nothing pending the hooks return without touching the queue.
// ---------------------------------------------------------------------------

namespace SOS::Hotkeys
{
	namespace
	{
		// The vtable slots of the two classes whose Poll produces the events we
		// want to keep away from the menu. Both are the third entry: ~dtor, then
		// the two inherited virtuals, then Poll.
		constexpr std::size_t kKeyboardPollSlot = 2;  // RE::BSWin32KeyboardDevice::Poll(float)
		constexpr std::size_t kGamepadPollSlot = 2;   // RE::BSPCGamepadDeviceHandler::Poll(float)

		// ---- FreeClimb, src/SettingsMenu.cpp: BindingCaptureOwnership ----------
		//
		// Which keys the recorder has taken off the queue and whose key-up still has
		// to be taken off as well. The menu never saw the press, so it must not see
		// the release either; that is what this remembers.
		class Ownership
		{
		public:
			static constexpr std::size_t kKeyboardCodes = 256;
			static constexpr std::size_t kGamepadCodes = 16;

			bool Filter(bool a_gamepad, std::uint32_t a_code, bool a_down, bool a_up, bool a_capturing)
			{
				const std::size_t index = static_cast<std::size_t>(a_code) + (a_gamepad ? kKeyboardCodes : 0);
				if (index >= kKeyboardCodes + kGamepadCodes) {
					return a_capturing;
				}
				const bool previous = _owned.test(index);
				if (a_up) {
					_owned.reset(index);
					return previous;
				}
				if (a_down) {
					_owned.set(index, a_capturing);
					return a_capturing;
				}
				return previous;
			}

			[[nodiscard]] bool Pending() const { return _owned.any(); }
			void               Clear() { _owned.reset(); }

		private:
			std::bitset<kKeyboardCodes + kGamepadCodes> _owned;
		};

		Ownership g_ownership;

		// ---- FreeClimb, src/Controls.h: removeInputEvents ----------------------
		//
		// Unlinks every event the predicate accepts from the engine's input queue.
		// The queue is a singly linked list with head and tail the engine keeps in
		// BSInputEventQueue; the walk mirrors FreeClimb's.
		template <class Predicate>
		void RemoveInputEvents(RE::InputEvent*& a_head, RE::InputEvent*& a_tail, Predicate a_remove)
		{
			RE::InputEvent* previous = nullptr;
			for (auto* event = a_head; event;) {
				auto* next = event->next;
				if (a_remove(event)) {
					if (previous) {
						previous->next = next;
					} else {
						a_head = next;
					}
					if (a_tail == event) {
						a_tail = previous;
					}
				} else {
					previous = event;
				}
				event = next;
			}
		}

		// ---- FreeClimb, src/SettingsMenu.cpp: settingsMenuFilterInput ---------
		[[nodiscard]] bool WantsEventRemoved(RE::InputEvent* a_event)
		{
			const bool capturing = CaptureActive();
			if (!capturing && !g_ownership.Pending()) {
				return false;
			}

			auto* button = a_event ? a_event->AsButtonEvent() : nullptr;
			if (!button) {
				return false;
			}
			const auto device = button->GetDevice();
			if (device != RE::INPUT_DEVICE::kKeyboard && device != RE::INPUT_DEVICE::kGamepad) {
				return false;
			}

			const bool    gamepad = device == RE::INPUT_DEVICE::kGamepad;
			std::uint32_t code = button->GetIDCode();
			if (gamepad) {
				// A controller button reports its mask; the filter works in XInput's
				// button order like FreeClimb, so the mask is turned back into a
				// keycode and the gamepad offset is removed.
				const auto keycode = SKSE::InputMap::GamepadMaskToKeycode(code);
				if (keycode < SKSE::InputMap::kMacro_GamepadOffset || keycode >= SKSE::InputMap::kMaxMacros) {
					return capturing;
				}
				code = keycode - SKSE::InputMap::kMacro_GamepadOffset;
			}
			return g_ownership.Filter(gamepad, code, button->IsDown(), button->IsUp(), capturing);
		}

		void FilterInputQueue()
		{
			if (!CaptureActive() && !g_ownership.Pending()) {
				return;
			}
			auto* queue = RE::BSInputEventQueue::GetSingleton();
			if (!queue) {
				return;
			}
			RemoveInputEvents(queue->GetQueueHead(), queue->GetQueueTail(), [](RE::InputEvent* a_event) {
				return WantsEventRemoved(a_event);
			});
		}

		// ---- FreeClimb, src/RuntimeSupport.h: the guards around a vtable write --
		[[nodiscard]] bool Committed(const void* a_address, std::uint32_t a_protect)
		{
			MEMORY_BASIC_INFORMATION info{};
			if (::VirtualQuery(a_address, &info, sizeof(info)) == 0) {
				return false;
			}
			if (info.State != MEM_COMMIT) {
				return false;
			}
			if ((info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
				return false;
			}
			return (info.Protect & a_protect) != 0;
		}

		[[nodiscard]] bool Readable(const void* a_address, std::size_t a_size)
		{
			return a_address && Committed(a_address, PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
														  PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY) &&
				   a_size != 0;
		}

		[[nodiscard]] bool Executable(const void* a_address)
		{
			return a_address && Committed(a_address, PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY);
		}

		template <class Fn>
		bool InstallVirtualSlot(REL::VariantID a_table, std::size_t a_slot, Fn a_replacement, Fn& a_original)
		{
			if (!a_replacement || a_slot > 0x1000) {
				return false;
			}
			REL::Relocation<std::uintptr_t> table{ a_table };
			const auto                      base = table.address();
			if (base == 0) {
				return false;
			}
			auto* location = reinterpret_cast<void**>(base + a_slot * sizeof(void*));
			if (!Readable(location, sizeof(void*))) {
				return false;
			}
			void* current = *location;
			if (!Executable(current)) {
				return false;
			}
			DWORD previous{ 0 };
			if (::VirtualProtect(location, sizeof(void*), PAGE_EXECUTE_READWRITE, &previous) == 0) {
				return false;
			}
			void* replaced = ::InterlockedCompareExchangePointer(location, reinterpret_cast<void*>(a_replacement), current);
			DWORD restored{ 0 };
			::VirtualProtect(location, sizeof(void*), previous, &restored);
			if (replaced != current) {
				SKSE::log::warn("Hotkeys: another plugin wrote the input vtable slot first; the menu keeps its keys.");
				return false;
			}
			a_original = reinterpret_cast<Fn>(current);
			return true;
		}

		// ---- FreeClimb, src/Plugin.cpp: the two device hooks ------------------
		using KeyboardPollFn = void (*)(RE::BSWin32KeyboardDevice*, float);
		using GamepadPollFn = void (*)(RE::BSPCGamepadDeviceHandler*, float);

		KeyboardPollFn g_keyboardPoll{ nullptr };
		GamepadPollFn  g_gamepadPoll{ nullptr };
		bool           g_filtersInstalled{ false };

		void KeyboardPollHook(RE::BSWin32KeyboardDevice* a_device, float a_timeDelta)
		{
			if (g_keyboardPoll) {
				g_keyboardPoll(a_device, a_timeDelta);
			}
			// After the device has filled the queue and before anything is dispatched.
			FilterInputQueue();
		}

		void GamepadPollHook(RE::BSPCGamepadDeviceHandler* a_handler, float a_timeDelta)
		{
			if (g_gamepadPoll) {
				g_gamepadPoll(a_handler, a_timeDelta);
			}
			FilterInputQueue();
		}
	}

	void InstallInputFilters()
	{
		if (g_filtersInstalled) {
			return;
		}

		const bool keyboard = InstallVirtualSlot(RE::VTABLE_BSWin32KeyboardDevice[0], kKeyboardPollSlot, &KeyboardPollHook, g_keyboardPoll);
		const bool gamepad = InstallVirtualSlot(RE::VTABLE_BSPCGamepadDeviceHandler[0], kGamepadPollSlot, &GamepadPollHook, g_gamepadPoll);
		g_filtersInstalled = keyboard || gamepad;

		// A missing slot is not fatal: the recorder still works on its polled
		// snapshot, and Escape is still recognised from the framework's own events.
		if (keyboard) {
			SKSE::log::debug("Hotkeys: keyboard events are taken off the queue while a hotkey is recorded.");
		} else {
			SKSE::log::warn("Hotkeys: the keyboard poll slot could not be hooked; Escape will also reach the menu framework.");
		}
		if (gamepad) {
			SKSE::log::debug("Hotkeys: controller events are taken off the queue while a hotkey is recorded.");
		} else {
			SKSE::log::warn("Hotkeys: the controller poll slot could not be hooked; the pad's own buttons will also reach the menu framework.");
		}
	}

	void ResetInputOwnership()
	{
		g_ownership.Clear();
	}
}
