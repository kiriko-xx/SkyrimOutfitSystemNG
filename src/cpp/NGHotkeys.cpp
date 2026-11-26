#include "NGHotkeys.h"

#include "I18n.h"

#include "AutoSwitch.h"
#include "MenuBridge.h"
#include "MenuUI.h"
#include "MorphRefresh.h"
#include "SceneOutfits.h"
#include "compat/ModCompat.h"
#include "core/Settings.h"
#include "input/FreeClimbCapture.h"

#include <algorithm>
#include <atomic>
#include <bitset>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "NGFeatures.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <Windows.h>

namespace SOS::Hotkeys
{
	namespace
	{
		// A hotkey is a chord: every trigger has to be down at the same time.
		// Keyboard triggers use DirectInput scan codes (what ButtonEvent reports and
		// what the keyboard polling below produces); the modifier keys are kept as
		// pseudo codes so that the ini text stays readable ("ctrl", not "lctrl") and
		// the old ini values keep working. Controller triggers use the button masks
		// ButtonEvent reports (0x0001 .. 0x8000, plus 0x0009/0x000A for the
		// triggers).
		enum class Dev : std::uint8_t
		{
			kKeyboard,
			kGamepad
		};

		struct Trigger
		{
			Dev           device{ Dev::kKeyboard };
			std::uint32_t code{ 0 };
		};

		constexpr std::uint32_t kModCtrl = 0x00010000u;
		constexpr std::uint32_t kModShift = 0x00020000u;
		constexpr std::uint32_t kModAlt = 0x00030000u;

		bool Same(const Trigger& a_lhs, const Trigger& a_rhs)
		{
			return a_lhs.device == a_rhs.device && a_lhs.code == a_rhs.code;
		}

		bool Contains(const std::vector<Trigger>& a_list, const Trigger& a_trigger)
		{
			return std::any_of(a_list.begin(), a_list.end(), [&](const Trigger& t) { return Same(t, a_trigger); });
		}

		struct Chord
		{
			std::vector<Trigger> triggers;

			bool valid() const { return !triggers.empty(); }
		};

		// ---- one table per device -------------------------------------------
		//
		// Everything about a keyboard key lives in one place: its scan code (what
		// the game reports), the virtual key (what GetAsyncKeyState wants) and the
		// text used in the ini. Reading a name back and printing a recorded key both
		// go through this table, so a recorded key always matches the key that is
		// later pressed - that was not true when the two directions had their own
		// hand written tables.
		struct KeyEntry
		{
			std::uint32_t scan;
			int           vk;
			const char*   name;
		};

		const std::vector<KeyEntry>& KeyTable()
		{
			static const std::vector<KeyEntry> table{
				{ 0x01, VK_ESCAPE, "esc" },
				{ 0x02, '1', "1" }, { 0x03, '2', "2" }, { 0x04, '3', "3" }, { 0x05, '4', "4" }, { 0x06, '5', "5" },
				{ 0x07, '6', "6" }, { 0x08, '7', "7" }, { 0x09, '8', "8" }, { 0x0A, '9', "9" }, { 0x0B, '0', "0" },
				{ 0x0C, VK_OEM_MINUS, "-" },
				{ 0x0D, VK_OEM_PLUS, "=" },
				{ 0x0E, VK_BACK, "backspace" },
				{ 0x0F, VK_TAB, "tab" },
				{ 0x10, 'Q', "q" }, { 0x11, 'W', "w" }, { 0x12, 'E', "e" }, { 0x13, 'R', "r" }, { 0x14, 'T', "t" },
				{ 0x15, 'Y', "y" }, { 0x16, 'U', "u" }, { 0x17, 'I', "i" }, { 0x18, 'O', "o" }, { 0x19, 'P', "p" },
				{ 0x1A, VK_OEM_4, "[" },
				{ 0x1B, VK_OEM_6, "]" },
				{ 0x1C, VK_RETURN, "enter" },
				{ kModCtrl, VK_CONTROL, "ctrl" },
				{ 0x1E, 'A', "a" }, { 0x1F, 'S', "s" }, { 0x20, 'D', "d" }, { 0x21, 'F', "f" }, { 0x22, 'G', "g" },
				{ 0x23, 'H', "h" }, { 0x24, 'J', "j" }, { 0x25, 'K', "k" }, { 0x26, 'L', "l" },
				{ 0x27, VK_OEM_1, ";" },
				{ 0x28, VK_OEM_7, "'" },
				{ 0x29, VK_OEM_3, "grave" },
				{ kModShift, VK_SHIFT, "shift" },
				{ 0x2B, VK_OEM_5, "\\" },
				{ 0x2C, 'Z', "z" }, { 0x2D, 'X', "x" }, { 0x2E, 'C', "c" }, { 0x2F, 'V', "v" }, { 0x30, 'B', "b" },
				{ 0x31, 'N', "n" }, { 0x32, 'M', "m" },
				{ 0x33, VK_OEM_COMMA, "," },
				{ 0x34, VK_OEM_PERIOD, "." },
				{ 0x35, VK_OEM_2, "/" },
				{ 0x37, VK_MULTIPLY, "num*" },
				{ kModAlt, VK_MENU, "alt" },
				{ 0x39, VK_SPACE, "space" },
				{ 0x3A, VK_CAPITAL, "capslock" },
				{ 0x3B, VK_F1, "f1" }, { 0x3C, VK_F2, "f2" }, { 0x3D, VK_F3, "f3" }, { 0x3E, VK_F4, "f4" },
				{ 0x3F, VK_F5, "f5" }, { 0x40, VK_F6, "f6" }, { 0x41, VK_F7, "f7" }, { 0x42, VK_F8, "f8" },
				{ 0x43, VK_F9, "f9" }, { 0x44, VK_F10, "f10" },
				{ 0x45, VK_NUMLOCK, "numlock" },
				{ 0x46, VK_SCROLL, "scrolllock" },
				{ 0x47, VK_NUMPAD7, "num7" }, { 0x48, VK_NUMPAD8, "num8" }, { 0x49, VK_NUMPAD9, "num9" },
				{ 0x4A, VK_SUBTRACT, "num-" },
				{ 0x4B, VK_NUMPAD4, "num4" }, { 0x4C, VK_NUMPAD5, "num5" }, { 0x4D, VK_NUMPAD6, "num6" },
				{ 0x4E, VK_ADD, "num+" },
				{ 0x4F, VK_NUMPAD1, "num1" }, { 0x50, VK_NUMPAD2, "num2" }, { 0x51, VK_NUMPAD3, "num3" },
				{ 0x52, VK_NUMPAD0, "num0" },
				{ 0x53, VK_DECIMAL, "num." },
				{ 0x57, VK_F11, "f11" }, { 0x58, VK_F12, "f12" },
				{ 0xB5, VK_DIVIDE, "num/" },
				{ 0xC7, VK_HOME, "home" }, { 0xC8, VK_UP, "up" }, { 0xC9, VK_PRIOR, "pgup" },
				{ 0xCB, VK_LEFT, "left" }, { 0xCD, VK_RIGHT, "right" }, { 0xCF, VK_END, "end" },
				{ 0xD0, VK_DOWN, "down" }, { 0xD1, VK_NEXT, "pgdn" }, { 0xD2, VK_INSERT, "insert" },
				{ 0xD3, VK_DELETE, "delete" }
			};
			return table;
		}

		struct PadEntry
		{
			std::uint32_t code;
			const char*   name;
		};

		const std::vector<PadEntry>& PadTable()
		{
			using Keys = RE::BSWin32GamepadDevice::Keys;
			static const std::vector<PadEntry> table{
				{ Keys::kUp, "up" },
				{ Keys::kDown, "down" },
				{ Keys::kLeft, "left" },
				{ Keys::kRight, "right" },
				{ Keys::kStart, "start" },
				{ Keys::kBack, "back" },
				{ Keys::kLeftThumb, "lstick" },
				{ Keys::kRightThumb, "rstick" },
				{ Keys::kLeftShoulder, "lb" },
				{ Keys::kRightShoulder, "rb" },
				{ Keys::kA, "a" },
				{ Keys::kB, "b" },
				{ Keys::kX, "x" },
				{ Keys::kY, "y" },
				{ Keys::kLeftTrigger, "lt" },
				{ Keys::kRightTrigger, "rt" }
			};
			return table;
		}

		std::uint32_t ScanCodeFor(std::string_view a_token)
		{
			for (const auto& entry : KeyTable()) {
				if (a_token == entry.name) {
					return entry.scan;
				}
			}
			return 0;
		}

		std::string_view NameForScan(std::uint32_t a_code)
		{
			for (const auto& entry : KeyTable()) {
				if (entry.scan == a_code) {
					return entry.name;
				}
			}
			return {};
		}

		// The virtual key GetAsyncKeyState wants for a scan code, or 0 when the
		// table does not name that key.
		int VkForScan(std::uint32_t a_code)
		{
			for (const auto& entry : KeyTable()) {
				if (entry.scan == a_code) {
					return entry.vk;
				}
			}
			return 0;
		}

		std::uint32_t GamepadCodeFor(std::string_view a_token)
		{
			for (const auto& entry : PadTable()) {
				if (a_token == entry.name) {
					return entry.code;
				}
			}
			return 0;
		}

		std::string_view GamepadNameFor(std::uint32_t a_code)
		{
			for (const auto& entry : PadTable()) {
				if (entry.code == a_code) {
					return entry.name;
				}
			}
			return {};
		}

		// The modifier keys have dedicated scan codes; fold the left and the right
		// one onto the same trigger so the ini text stays identical.
		std::uint32_t ModifierForScan(std::uint32_t a_code)
		{
			switch (a_code) {
			case 0x1D:  // left ctrl
			case 0x9D:  // right ctrl
				return kModCtrl;
			case 0x2A:  // left shift
			case 0x36:  // right shift
				return kModShift;
			case 0x38:  // left alt
			case 0xB8:  // right alt
				return kModAlt;
			default:
				return 0;
			}
		}

		std::string Format(const Chord& a_chord)
		{
			std::string text;
			for (const auto& trigger : a_chord.triggers) {
				if (!text.empty()) {
					text += '+';
				}
				if (trigger.device == Dev::kGamepad) {
					const auto name = GamepadNameFor(trigger.code);
					text += "pad:";
					text += name.empty() ? "?" : std::string(name);
					continue;
				}
				const auto name = NameForScan(trigger.code);
				if (name.empty()) {
					text += "?";
				} else if (name.size() == 1 && std::isalpha(static_cast<unsigned char>(name.front()))) {
					text += static_cast<char>(std::toupper(static_cast<unsigned char>(name.front())));
				} else {
					text += name;
				}
			}
			return text;
		}

		std::string_view Trim(std::string_view a_text)
		{
			while (!a_text.empty() && std::isspace(static_cast<unsigned char>(a_text.front()))) {
				a_text.remove_prefix(1);
			}
			while (!a_text.empty() && std::isspace(static_cast<unsigned char>(a_text.back()))) {
				a_text.remove_suffix(1);
			}
			return a_text;
		}

		std::string_view Lower(std::string_view a_text, std::string& a_storage)
		{
			a_storage.assign(a_text);
			std::transform(a_storage.begin(), a_storage.end(), a_storage.begin(), [](unsigned char c) {
				return static_cast<char>(std::tolower(c));
			});
			return a_storage;
		}

		Chord Parse(const std::string& a_text, const char* a_name)
		{
			Chord            result;
			std::string_view text = Trim(a_text);
			if (text.empty()) {
				return result;
			}

			std::vector<std::string_view> tokens;
			std::size_t                   start = 0;
			for (std::size_t i = 0; i <= text.size(); ++i) {
				if (i == text.size() || text[i] == '+') {
					tokens.push_back(Trim(text.substr(start, i - start)));
					start = i + 1;
				}
			}

			std::string storage;
			for (const auto token : tokens) {
				if (token.empty()) {
					continue;
				}
				const auto lower = Lower(token, storage);
				Trigger    trigger;
				if (lower.rfind("pad:", 0) == 0) {
					trigger.device = Dev::kGamepad;
					trigger.code = GamepadCodeFor(lower.substr(4));
				} else {
					trigger.code = ScanCodeFor(lower);
				}

				if (trigger.code == 0) {
					SKSE::log::warn("Hotkeys: cannot parse '{}' in the {} hotkey '{}'; that hotkey stays disabled.", token, a_name, a_text);
					return Chord{};
				}
				if (!Contains(result.triggers, trigger)) {
					result.triggers.push_back(trigger);
				}
			}

			if (result.triggers.size() > kMaxChordKeys) {
				SKSE::log::warn("Hotkeys: the {} hotkey '{}' names {} keys; only the first {} are used.", a_name, a_text, result.triggers.size(), kMaxChordKeys);
				result.triggers.resize(kMaxChordKeys);
			}
			return result;
		}

		// ---- state ------------------------------------------------------------
		//
		// Every hotkey can be bound twice, once to the keyboard and once to the
		// controller, and both bindings stay active at the same time.
		Chord      g_randomKb;
		Chord      g_randomPad;
		Chord      g_quickKb;
		Chord      g_quickPad;
		Chord      g_quickKey;
		std::mutex g_chordMutex;

		// Everything currently held down, maintained from the input sink, which runs
		// on the device thread.
		std::vector<Trigger> g_held;
		std::mutex           g_heldMutex;

		bool g_latchedRandom{ false };
		bool g_latchedQuick{ false };
		bool g_latchedQuickKey{ false };

		bool              g_installed{ false };
		std::atomic<bool> g_textInput{ false };
		std::atomic<std::int64_t> g_textInputStamp{ 0 };

		// The framework tells us when its menu opens and closes. While it is open the
		// keys belong to the menu: a hotkey that fired there would apply an outfit
		// from inside the settings page.
		std::atomic<bool> g_frameworkMenuOpen{ false };

		// A recording owns the input: nothing may fire while it runs, and the host
		// must not see the keys that are being recorded.
		std::atomic<bool> g_capturing{ false };

		std::atomic<std::int64_t> g_lastQuickslotOpenMs{ 0 };

		// The quickslot power only opens the window once its cast animation has
		// finished, which feels like a cooldown on the Z key. The mirrored key skips
		// that wait and is limited to one open every 0.5 s.
		constexpr std::int64_t kQuickslotOpenCooldownMs = 500;

		// The random outfit and the power's random roll answer on the key press, so
		// they need their own 0.5 s gate instead of waiting for the power to finish
		// casting. g_lastPowerActionMs is what tells the Papyrus side that this press
		// has already been served.
		std::atomic<std::uint64_t> g_lastPowerActionMs{ 0 };
		constexpr std::uint64_t    kRandomCooldownMs = 500;
		std::atomic<std::uint64_t> g_lastRandomFireMs{ 0 };

		std::int64_t NowMs()
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now().time_since_epoch()).count();
		}

		bool CachedTextInputActive()
		{
			if (!g_textInput.load(std::memory_order_relaxed)) {
				return false;
			}
			// The menu refreshes this timestamp from the render thread every frame.
			// Expiring it avoids leaving hotkeys blocked when the menu closes while an
			// input box still had focus on the last rendered frame.
			const auto now = NowMs();
			const auto stamp = g_textInputStamp.load(std::memory_order_relaxed);
			return stamp != 0 && now - stamp < 500;
		}

		// ---- the gate ---------------------------------------------------------
		//
		// A hotkey may only act while the player is actually playing. The previous
		// build only looked at the console and at text fields, which is why the keys
		// kept firing from inside the settings page.

		bool GameWindowFocused()
		{
			const auto foreground = ::GetForegroundWindow();
			if (!foreground) {
				return false;
			}
			DWORD pid = 0;
			::GetWindowThreadProcessId(foreground, &pid);
			return pid == ::GetCurrentProcessId();
		}

		bool OurMenuOpen()
		{
			// The quick picker is an in-game window: its own hotkey has to keep working
			// so the same key can close it again.
			if (Menu::QuickPickerVisible()) {
				return false;
			}
			if (g_frameworkMenuOpen.load(std::memory_order_relaxed)) {
				return true;
			}
			return MenuBridge::AnyBlockingWindowOpened();
		}

		bool FiringBlocked()
		{
			if (g_capturing.load(std::memory_order_relaxed)) {
				return true;
			}
			if (CachedTextInputActive()) {
				return true;
			}
			if (auto* ui = RE::UI::GetSingleton()) {
				if (ui->IsMenuOpen(RE::Console::MENU_NAME)) {
					return true;
				}
				if (ui->GameIsPaused()) {
					return true;
				}
			}
			if (OurMenuOpen()) {
				return true;
			}
			return !GameWindowFocused();
		}

		// Defined with the pad polling further down; declared here because the
		// chord checks below are what asks for it.
		bool PadDown(const Trigger& a_trigger);

		// True while the trigger is physically down.
		//
		// Neither device is asked "did the sink see a key-down?" any more. While one
		// of our own windows is open the game stops handing button events to plugins,
		// so the key-up is eaten with them and a key that was remembered as held
		// stayed in the chord for the rest of the session: a recorded Alt+O then
		// fired on Alt alone as soon as the quick picker had been opened once, and a
		// recorded LB+up fired on LB alone.
		bool Down(const Trigger& a_trigger)
		{
			if (a_trigger.device == Dev::kGamepad) {
				return PadDown(a_trigger);
			}
			int vk = 0;
			if (a_trigger.code >= kModCtrl) {
				if (a_trigger.code == kModCtrl) {
					vk = VK_CONTROL;
				} else if (a_trigger.code == kModShift) {
					vk = VK_SHIFT;
				} else {
					vk = VK_MENU;
				}
			} else {
				vk = VkForScan(a_trigger.code);
			}
			if (vk != 0) {
				return (::GetAsyncKeyState(vk) & 0x8000) != 0;
			}
			// A key the table does not name: fall back to what the sink saw.
			std::lock_guard lock(g_heldMutex);
			return Contains(g_held, a_trigger);
		}

		bool AllDown(const Chord& a_chord)
		{
			if (a_chord.triggers.empty()) {
				return false;
			}
			for (const auto& trigger : a_chord.triggers) {
				if (!Down(trigger)) {
					return false;
				}
			}
			return true;
		}

		bool AnyChordDown(const Chord& a_first, const Chord& a_second)
		{
			return (a_first.valid() && AllDown(a_first)) || (a_second.valid() && AllDown(a_second));
		}

		bool AnyChordBound(const Chord& a_first, const Chord& a_second)
		{
			return a_first.valid() || a_second.valid();
		}

		// Keeps only the triggers of one device, so a binding written by an older
		// build (which kept both devices in a single ini value) can be folded into
		// the two fields.
		Chord OnlyDevice(const Chord& a_chord, Dev a_device)
		{
			Chord result;
			for (const auto& trigger : a_chord.triggers) {
				if (trigger.device == a_device) {
					result.triggers.push_back(trigger);
				}
			}
			return result;
		}

		void AppendChord(Chord& a_target, const Chord& a_source)
		{
			for (const auto& trigger : a_source.triggers) {
				if (!Contains(a_target.triggers, trigger)) {
					a_target.triggers.push_back(trigger);
				}
			}
			if (a_target.triggers.size() > kMaxChordKeys) {
				a_target.triggers.resize(kMaxChordKeys);
			}
		}

		void Remember(const Trigger& a_trigger, bool a_down)
		{
			std::lock_guard lock(g_heldMutex);
			if (a_down) {
				if (!Contains(g_held, a_trigger)) {
					g_held.push_back(a_trigger);
				}
			} else {
				g_held.erase(std::remove_if(g_held.begin(), g_held.end(), [&](const Trigger& t) { return Same(t, a_trigger); }), g_held.end());
			}
		}

		void ForgetAll()
		{
			std::lock_guard lock(g_heldMutex);
			g_held.clear();
		}

		void Fire()
		{
			// The input sink runs on the device manager's thread; actor updates and
			// HUD messages must be executed on the main thread.
			auto* tasks = SKSE::GetTaskInterface();
			if (!tasks) {
				return;
			}
			tasks->AddTask([]() {
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (!player) {
					return;
				}

				const auto& settings = Settings::Get();
				const auto  chosen = Features::PickRandomOutfit(player, settings.randomFavoritesOnly);
				if (chosen.empty()) {
					RE::SendHUDMessage::ShowHUDMessage(I18n::T("No outfits available to pick from.", "没有可供抽取的幻化。"), nullptr, false);
					return;
				}

				const auto message = std::string(I18n::T("Outfit: ", "幻化：")) + chosen;
				RE::SendHUDMessage::ShowHUDMessage(message.c_str(), nullptr, false);
				SKSE::log::debug("Hotkeys: random outfit '{}' applied.", chosen);
			});
		}

		void FireQuickPicker()
		{
			auto* tasks = SKSE::GetTaskInterface();
			if (!tasks) {
				return;
			}
			tasks->AddTask([]() {
				Menu::ToggleQuickPicker();
				SKSE::log::debug("Hotkeys: quick picker toggled (visible: {}).", Menu::QuickPickerVisible() ? "yes" : "no");
			});
		}

		// The shipped esp grants the "Change Outfit" lesser power. Pressing its
		// quickslot key only reaches the effect script after the cast animation of
		// the power, so the window shows up with a delay that users read as a
		// cooldown. This runs on the key press itself; the power then casts as usual
		// and its OpenQuickPicker() call does nothing because the window is already
		// open.
		void FireQuickslotKey()
		{
			auto* tasks = SKSE::GetTaskInterface();
			if (!tasks) {
				return;
			}
			tasks->AddTask([]() {
				const auto& settings = Settings::Get();
				// Either way the plugin does it now: waiting for the power's cast
				// animation was the "cooldown" users complained about. The effect script
				// that runs afterwards is skipped through PowerActionJustHandled().
				g_lastPowerActionMs.store(static_cast<std::uint64_t>(NowMs()), std::memory_order_relaxed);
				if (settings.quickslotRandom) {
					Fire();
					return;
				}
				if (Menu::QuickPickerVisible()) {
					return;
				}
				Menu::ShowQuickPicker(true);
				SKSE::log::debug("Hotkeys: quick picker opened by the game's own power key.");
			});
		}

		// ---- capture ----------------------------------------------------------
		//
		// The recorder is FreeClimb's, copied unchanged (see input/FreeClimbCapture.h):
		// it compares what is held right now against what it has collected so far and
		// stores a chord only once every key of it has been released again. That is what
		// makes Alt on its way to Alt+O harmless, what refuses keys that were pressed one
		// after the other, and what keeps the click that opened the window out of the
		// chord.
		//
		// While a recording runs the menu framework is told to stop reading input
		// (IG::SetMenuInputSuppressed) and the events this sink is handed are eaten, so
		// Escape, the controller's Back button and the shoulder buttons no longer act on
		// the menu behind the recorder. No device is hooked and the input queue is not
		// touched: the state is polled from Win32 and XInput, which is what keeps working
		// on every game build.

		enum class CaptureState : int
		{
			kIdle = 0,
			kWaiting,
			kArmed,
			kCaptured
		};

		enum class CaptureProblem : int
		{
			kNone = 0,
			kTooMany,
			kInvalid,
			kNotTogether
		};

		std::atomic<int>  g_captureSlot{ -1 };
		// 0 = keyboard, 1 = gamepad; see NGHotkeys::Input.
		std::atomic<int>  g_captureDevice{ 0 };
		std::atomic<int>  g_captureState{ 0 };
		std::atomic<int>  g_captureProblem{ 0 };
		std::mutex        g_captureMutex;
		std::vector<Trigger> g_captureCandidate;
		// The controller buttons of the running recording in the order they went
		// down: the window shows the chord the way it was pressed, not in the order
		// XInput numbers its buttons in (LB then the d-pad is "pad:lb+pad:up").
		// Guarded by g_captureMutex.
		std::vector<unsigned> g_captureOrder;
		std::string          g_captureResult;
		int                  g_captureResultSlot{ -1 };
		int                  g_captureResultDevice{ 0 };
		std::int64_t         g_captureStartedMs{ 0 };
		// A recording that is left open (the player walked away) must not keep the
		// host from reading input for the rest of the session.
		constexpr std::int64_t kCaptureTimeoutMs = 20000;

		// FreeClimb's state machine for the running recording; g_captureMutex guards it.
		fc::BindingCapture g_capture;

		Dev CaptureDeviceDev()
		{
			return g_captureDevice.load(std::memory_order_relaxed) == 0 ? Dev::kKeyboard : Dev::kGamepad;
		}

		// Esc and the controller's Back button end a recording without storing
		// anything.
		bool IsCaptureCancel(const Trigger& a_trigger)
		{
			if (a_trigger.device == Dev::kKeyboard) {
				return a_trigger.code == 0x01;
			}
			return a_trigger.code == RE::BSWin32GamepadDevice::Keys::kBack;
		}

		// True when the event asks to end the recording (Esc on the keyboard, Back on
		// the controller). The queue filter normally takes those events out before
		// they are dispatched; this is the path that still works when the device hooks
		// could not be installed.
		bool EventAsksCancel(RE::InputEvent* a_event)
		{
			auto* button = a_event ? a_event->AsButtonEvent() : nullptr;
			if (!button || !button->IsDown()) {
				return false;
			}
			const auto device = button->GetDevice();
			if (device != RE::INPUT_DEVICE::kKeyboard && device != RE::INPUT_DEVICE::kGamepad) {
				return false;
			}
			Trigger trigger;
			trigger.device = device == RE::INPUT_DEVICE::kGamepad ? Dev::kGamepad : Dev::kKeyboard;
			trigger.code = button->GetIDCode();
			return IsCaptureCancel(trigger);
		}

		// ---- converting between FreeClimb's numbering and this plugin's ------
		//
		// FreeClimb numbers the pad buttons in the order XInput reports them and keeps
		// the two triggers as slots 14 and 15; the ini, the input events and the pad
		// keys of this plugin use the masks the engine reports instead.
		constexpr std::uint32_t kPadButtons[] = {
			0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080,
			0x0100, 0x0200, 0x1000, 0x2000, 0x4000, 0x8000
		};
		// An analogue trigger counts as pressed from this value on.
		constexpr std::uint8_t kPadTriggerOn = 30;

		std::uint32_t GamepadCodeForIndex(unsigned a_index)
		{
			if (a_index < 14) {
				return kPadButtons[a_index];
			}
			return a_index == 14 ? RE::BSWin32GamepadDevice::Keys::kLeftTrigger : RE::BSWin32GamepadDevice::Keys::kRightTrigger;
		}

		// FreeClimb keeps the physical left and right modifier apart; the ini folds them
		// onto one trigger, so a recorded chord is folded the same way.
		std::uint32_t KeyCodeForTrigger(fc::KeyCode a_key)
		{
			switch (fc::keyGroup(a_key)) {
			case fc::anyShift:
				return kModShift;
			case fc::anyControl:
				return kModCtrl;
			case fc::anyAlt:
				return kModAlt;
			default:
				return static_cast<std::uint32_t>(a_key);
			}
		}

		// Called with g_captureMutex held (it reads g_captureOrder).
		std::vector<Trigger> TriggersOf(const fc::BindingCaptureResult& a_result)
		{
			std::vector<Trigger> triggers;
			if (a_result.device == fc::BindingCaptureDevice::gamepad) {
				// The order the buttons went down in comes first; anything the order
				// does not know yet follows by bit number.
				for (const auto index : g_captureOrder) {
					if (index < 16 && (a_result.gamepad & fc::gamepadButtonMask(index)) != 0) {
						triggers.push_back(Trigger{ Dev::kGamepad, GamepadCodeForIndex(index) });
					}
				}
				for (unsigned i = 0; i < 16; ++i) {
					if ((a_result.gamepad & fc::gamepadButtonMask(i)) == 0) {
						continue;
					}
					if (std::find(g_captureOrder.begin(), g_captureOrder.end(), i) != g_captureOrder.end()) {
						continue;
					}
					triggers.push_back(Trigger{ Dev::kGamepad, GamepadCodeForIndex(i) });
				}
				return triggers;
			}
			for (unsigned i = 0; i < a_result.keyboard.count; ++i) {
				triggers.push_back(Trigger{ Dev::kKeyboard, KeyCodeForTrigger(a_result.keyboard.keys[i]) });
			}
			return triggers;
		}

		int ProblemOf(fc::BindingCaptureError a_error)
		{
			switch (a_error) {
			case fc::BindingCaptureError::tooMany:
				return static_cast<int>(CaptureProblem::kTooMany);
			case fc::BindingCaptureError::invalid:
				return static_cast<int>(CaptureProblem::kInvalid);
			case fc::BindingCaptureError::notSimultaneous:
				return static_cast<int>(CaptureProblem::kNotTogether);
			default:
				return static_cast<int>(CaptureProblem::kNone);
			}
		}

		// Called with g_captureMutex held: remembers the order in which the buttons of
		// a controller chord went down.
		void RememberCaptureOrderLocked(const fc::BindingCaptureResult& a_result)
		{
			if (a_result.device != fc::BindingCaptureDevice::gamepad) {
				return;
			}
			for (unsigned i = 0; i < 16; ++i) {
				if ((a_result.gamepad & fc::gamepadButtonMask(i)) == 0) {
					continue;
				}
				if (std::find(g_captureOrder.begin(), g_captureOrder.end(), i) == g_captureOrder.end()) {
					g_captureOrder.push_back(i);
				}
			}
		}

		// Called with g_captureMutex held: mirrors what the recorder holds right now so
		// that the window can show it.
		void SyncCaptureLocked(const fc::BindingCaptureResult& a_result)
		{
			g_captureCandidate = TriggersOf(a_result);
			g_captureState.store(static_cast<int>(a_result.status == fc::BindingCaptureStatus::armed ? CaptureState::kArmed : CaptureState::kWaiting),
				std::memory_order_relaxed);
		}

		// Called with g_captureMutex held: the chord is complete and every key of it has
		// been released again.
		void StoreCaptureLocked(const fc::BindingCaptureResult& a_result)
		{
			g_captureCandidate = TriggersOf(a_result);
			Chord chord;
			chord.triggers = g_captureCandidate;
			g_captureResult = Format(chord);
			g_captureResultSlot = g_captureSlot.load(std::memory_order_relaxed);
			g_captureResultDevice = a_result.device == fc::BindingCaptureDevice::gamepad ? 1 : 0;
			g_captureProblem.store(static_cast<int>(CaptureProblem::kNone), std::memory_order_relaxed);
			g_captureState.store(static_cast<int>(CaptureState::kCaptured), std::memory_order_relaxed);
			SKSE::log::debug("Hotkeys: recorded '{}' for the {} device.", g_captureResult, g_captureResultDevice == 0 ? "keyboard" : "controller");
		}
		void CheckCaptureTimeout()
		{
			if (g_captureStartedMs == 0) {
				return;
			}
			if (NowMs() - g_captureStartedMs < kCaptureTimeoutMs) {
				return;
			}
			SKSE::log::debug("Hotkeys: the hotkey recording timed out; nothing was stored.");
			CancelCapture();
		}

		// ---- polling ----------------------------------------------------------
		//
		// While the framework menu is open the game stops handing button events to
		// plugins, so the recorder would see nothing. The poll feeds the recorder
		// only: it never fires a hotkey, because the keys that reach it were typed
		// into the menu.

		struct PadState
		{
			std::uint32_t packet;
			// XINPUT_GAMEPAD starts with wButtons, a WORD, so the two trigger axes
			// sit at offsets 6 and 7. Reading the buttons as a DWORD moved both
			// triggers onto the first stick axis and made LT and RT look released
			// for ever, which is why they could not be recorded.
			std::uint16_t buttons;
			std::uint8_t  leftTrigger;
			std::uint8_t  rightTrigger;
			std::int16_t  thumbs[4];
		};
		static_assert(sizeof(PadState) == 16, "PadState has to match XINPUT_STATE, which XInputGetState writes");

		bool ReadPad(PadState& a_state)
		{
			using GetStateFn = std::uint32_t(__stdcall*)(std::uint32_t, PadState*);
			static GetStateFn getState = []() -> GetStateFn {
				for (const auto* name : { L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll" }) {
					if (auto* module = ::LoadLibraryW(name)) {
						if (auto* fn = reinterpret_cast<GetStateFn>(::GetProcAddress(module, "XInputGetState"))) {
							return fn;
						}
					}
				}
				return nullptr;
			}();
			if (!getState) {
				return false;
			}
			// Which slot the pad sits in is not fixed (a wheel or a virtual device
			// can take slot 0), so ask every index instead of only the first one.
			for (std::uint32_t index = 0; index < 4; ++index) {
				PadState state{};
				if (getState(index, &state) == 0) {
					a_state = state;
					return true;
				}
			}
			return false;
		}

		struct PadSnapshot
		{
			PadState state{};
			bool     valid{};
		};

		// The pad as XInput reports it right now, cached for a few milliseconds
		// because every chord check asks for it once per button event.
		PadSnapshot PhysicalPad()
		{
			static std::mutex    cacheMutex;
			static PadState      last{};
			static bool          lastValid = false;
			static std::uint64_t at = 0;
			std::lock_guard      lock(cacheMutex);
			const auto           now = NowMs();
			if (at != 0 && now - static_cast<std::int64_t>(at) < 8) {
				return PadSnapshot{ last, lastValid };
			}
			at = static_cast<std::uint64_t>(now);
			PadState pad{};
			if (ReadPad(pad)) {
				last = pad;
				lastValid = true;
			}
			// A dropped XInput read must not look like "every button released".
			return PadSnapshot{ last, lastValid };
		}

		// True while the controller button (or trigger) is physically held.
		//
		// The two triggers are compared as axes, not as bits of the button mask:
		// the engine code of the left trigger is 0x0009, which also contains the
		// d-pad bits 0x0001 and 0x0008.
		bool PadDown(const Trigger& a_trigger)
		{
			using Keys     = RE::BSWin32GamepadDevice::Keys;
			const auto pad = PhysicalPad();
			if (!pad.valid) {
				return false;
			}
			if (a_trigger.code == Keys::kLeftTrigger) {
				return pad.state.leftTrigger > kPadTriggerOn;
			}
			if (a_trigger.code == Keys::kRightTrigger) {
				return pad.state.rightTrigger > kPadTriggerOn;
			}
			return (pad.state.buttons & a_trigger.code) != 0;
		}

		// The physical modifier keys: the ini folds them onto one trigger, so the key
		// table carries them only as pseudo codes, but the recorder has to see them.
		struct ModifierEntry
		{
			std::uint32_t scan;
			int           vk;
		};

		constexpr ModifierEntry kModifierKeys[] = {
			{ 0x1D, VK_LCONTROL }, { 0x9D, VK_RCONTROL },
			{ 0x2A, VK_LSHIFT },   { 0x36, VK_RSHIFT },
			{ 0x38, VK_LMENU },    { 0xB8, VK_RMENU }
		};

		// One snapshot of both devices in the shape FreeClimb's recorder wants. It is
		// polled instead of hooked, so no engine address is involved.
		void PollDevices(fc::BindingCaptureSnapshot& a_snapshot)
		{
			a_snapshot.keyboard.reset();
			for (const auto& entry : KeyTable()) {
				if (entry.scan >= 0x100) {
					// The modifier groups are pseudo codes, not scan codes.
					continue;
				}
				a_snapshot.keyboard.set(entry.scan, (::GetAsyncKeyState(entry.vk) & 0x8000) != 0);
			}
			for (const auto& entry : kModifierKeys) {
				a_snapshot.keyboard.set(entry.scan, (::GetAsyncKeyState(entry.vk) & 0x8000) != 0);
			}
			a_snapshot.keyboardValid = true;

			static PadState lastPad{};
			static bool     lastPadValid = false;
			PadState        pad{};
			if (ReadPad(pad)) {
				lastPad = pad;
				lastPadValid = true;
			} else if (lastPadValid) {
				// Keep the last known state: a dropped XInput read must not look like
				// "every button released" in the middle of a chord.
				pad = lastPad;
			}
			a_snapshot.gamepadValid = lastPadValid;
			a_snapshot.gamepad = 0;
			if (lastPadValid) {
				for (unsigned i = 0; i < 14; ++i) {
					if ((pad.buttons & kPadButtons[i]) != 0) {
						a_snapshot.gamepad = static_cast<fc::GamepadChord>(a_snapshot.gamepad | fc::gamepadButtonMask(i));
					}
				}
				if (pad.leftTrigger > kPadTriggerOn) {
					a_snapshot.gamepad = static_cast<fc::GamepadChord>(a_snapshot.gamepad | fc::gamepadButtonMask(14));
				}
				if (pad.rightTrigger > kPadTriggerOn) {
					a_snapshot.gamepad = static_cast<fc::GamepadChord>(a_snapshot.gamepad | fc::gamepadButtonMask(15));
				}
			}

			// The click, the button or the key that opened the recorder is still down when
			// the first snapshot is taken; FreeClimb refuses to arm while anything is held,
			// which is how that press stays out of the chord.
			const auto mouseDown = [](int a_vk) { return (::GetAsyncKeyState(a_vk) & 0x8000) != 0; };
			bool       neutral = a_snapshot.gamepad == 0 && !mouseDown(VK_LBUTTON) && !mouseDown(VK_RBUTTON);
			for (const bool down : a_snapshot.keyboard.down) {
				neutral = neutral && !down;
			}
			a_snapshot.activationHeld = !neutral;
		}

		// Runs from the render thread every frame while the recorder is open: one
		// snapshot, one call into FreeClimb's state machine, and the heartbeat that ends
		// a recording nobody finishes.
		void PollCapture()
		{
			if (!g_capturing.load(std::memory_order_relaxed)) {
				return;
			}

			fc::BindingCaptureSnapshot snapshot;
			PollDevices(snapshot);

			bool captured = false;
			bool cancelled = false;
			{
				std::lock_guard                   lock(g_captureMutex);
				const fc::BindingCaptureResult& result = g_capture.sample(snapshot);
				RememberCaptureOrderLocked(result);
				switch (result.status) {
				case fc::BindingCaptureStatus::captured:
					StoreCaptureLocked(result);
					captured = true;
					// The recording is over: stop taking the player's keys off the queue
					// right away instead of on the next frame, when the page has already
					// picked the result up.
					g_capturing.store(false, std::memory_order_relaxed);
					break;
				case fc::BindingCaptureStatus::cancelled:
					cancelled = true;
					break;
				case fc::BindingCaptureStatus::error:
					// FreeClimb stops on a refused combination. Here the reason is shown in
					// the window and the recorder listens again, so the player can simply
					// press a different chord.
					g_captureProblem.store(ProblemOf(result.error), std::memory_order_relaxed);
					g_captureCandidate.clear();
					g_captureOrder.clear();
					g_captureState.store(static_cast<int>(CaptureState::kWaiting), std::memory_order_relaxed);
					g_capture.begin(g_captureDevice.load(std::memory_order_relaxed) == 0 ? fc::BindingCaptureDevice::keyboard : fc::BindingCaptureDevice::gamepad);
					break;
				default:
					SyncCaptureLocked(result);
					break;
				}
			}
			if (cancelled) {
				SKSE::log::debug("Hotkeys: the recording was cancelled; nothing was stored.");
				CancelCapture();
				return;
			}
			if (captured) {
				return;
			}
			CheckCaptureTimeout();
		}

		bool IsHeld(const Trigger& a_trigger)
		{
			return Down(a_trigger);
		}

		// The key the player has actually bound to Shout/Power in the game settings.
		// The "Change Outfit" lesser power is cast with it, so mirroring that binding
		// keeps the plugin in step with keyboard and controller layouts alike.
		//
		// ControlMap reports controller bindings in SKSE's input-keycode space
		// (266 + button index); everything else in this file works with the button
		// masks ButtonEvent reports, so the value is converted once here.
		bool GamePowerKeys(Trigger& a_keyboard, Trigger& a_gamepad)
		{
			static std::uint32_t cachedKeyboard = 0;
			static std::uint32_t cachedGamepad = 0;
			static std::uint32_t lastKeyboard = 0xFFFFFFFFu;
			static std::uint32_t lastGamepad = 0xFFFFFFFFu;
			static std::uint64_t cachedAt = 0;
			const auto           now = NowMs();
			if (cachedAt == 0 || now - static_cast<std::int64_t>(cachedAt) > 1000) {
				cachedAt = static_cast<std::uint64_t>(now);
				cachedKeyboard = 0;
				cachedGamepad = 0;
				if (auto* map = RE::ControlMap::GetSingleton()) {
					const auto keyboard = map->GetMappedKey("Shout", RE::INPUT_DEVICE::kKeyboard);
					const auto gamepad = map->GetMappedKey("Shout", RE::INPUT_DEVICE::kGamepad);
					if (keyboard != 0 && keyboard != 0xFF && keyboard < 0x100) {
						cachedKeyboard = keyboard;
					}
					if (gamepad != 0 && gamepad != 0xFF) {
						// The mapping is either already the button mask ButtonEvent
						// reports (0x0001 .. 0x8000, 0x0009/0x000A for the triggers)
						// or SKSE's macro keycode (266 .. 281). Accepting both shapes
						// is what keeps the controller half of the power key alive:
						// feeding a mask into the keycode conversion returns zero and
						// the controller binding silently disappears.
						std::uint32_t mask = gamepad;
						if (gamepad >= SKSE::InputMap::kMacro_GamepadOffset && gamepad < SKSE::InputMap::kMaxMacros) {
							mask = SKSE::InputMap::GamepadKeycodeToMask(gamepad);
						}
						if (mask != 0 && mask != 0xFF && mask <= 0xFFFF) {
							cachedGamepad = mask;
						}
					}
				}
			}
			if (cachedKeyboard != lastKeyboard || cachedGamepad != lastGamepad) {
				lastKeyboard = cachedKeyboard;
				lastGamepad = cachedGamepad;
				SKSE::log::debug("Hotkeys: the power key is 0x{:02X} on the keyboard and 0x{:04X} on the controller.", cachedKeyboard, cachedGamepad);
			}
			a_keyboard = Trigger{ Dev::kKeyboard, cachedKeyboard };
			a_gamepad = Trigger{ Dev::kGamepad, cachedGamepad };
			return cachedKeyboard != 0 || cachedGamepad != 0;
		}

		void FireRandomThrottled()
		{
			const auto now = NowMs();
			const auto last = g_lastRandomFireMs.load(std::memory_order_relaxed);
			if (last != 0 && now - static_cast<std::int64_t>(last) < static_cast<std::int64_t>(kRandomCooldownMs)) {
				return;
			}
			g_lastRandomFireMs.store(static_cast<std::uint64_t>(now), std::memory_order_relaxed);
			Fire();
		}

		// The Change Outfit power belongs to whatever plugin the player installed it
		// from, and the esp is an optional install: it may be missing, rebuilt or
		// renamed, and its scripts are optional as well. So nothing here searches the
		// load order or remembers a form id - the only question asked is which file the
		// power the player has actually selected comes from, which costs one lookup on
		// a pointer that is already there.
		constexpr std::string_view kPowerPlugin = "SkyrimOutfitSystem.esp";
		constexpr std::string_view kPowerPluginLight = "SkyrimOutfitSystem.esl";

		bool SameFilename(std::string_view a_left, std::string_view a_right)
		{
			if (a_left.size() != a_right.size()) {
				return false;
			}
			for (std::size_t i = 0; i < a_left.size(); ++i) {
				const auto left = static_cast<unsigned char>(a_left[i]);
				const auto right = static_cast<unsigned char>(a_right[i]);
				if (std::tolower(left) != std::tolower(right)) {
					return false;
				}
			}
			return true;
		}

		// True while the "Change Outfit" lesser power is the one the player has
		// selected in the magic menu. Pressing the power key is ours to handle only
		// then: with no power selected, or with any other power equipped, the key
		// belongs to the game and must be passed through untouched.
		bool OurPowerSelected()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return false;
			}
			auto* power = player->GetActorRuntimeData().selectedPower;
			if (!power) {
				return false;
			}
			auto* file = power->GetFile(0);
			if (!file) {
				return false;  // a form the game made up, not something the player installed
			}
			const std::string_view filename = file->GetFilename();
			if (!SameFilename(filename, kPowerPlugin) && !SameFilename(filename, kPowerPluginLight)) {
				return false;
			}
			static bool logged = false;
			if (!logged) {
				logged = true;
				SKSE::log::debug("Hotkeys: the Change Outfit power is {:08X}.", power->GetFormID());
			}
			return true;
		}

		// True when the trigger is one of the keys the game has bound to Shout/Power,
		// keyboard or controller.
		bool IsGamePowerKey(const Trigger& a_trigger)
		{
			Trigger keyboard{};
			Trigger gamepad{};
			if (!GamePowerKeys(keyboard, gamepad)) {
				return false;
			}
			return (keyboard.code != 0 && Same(a_trigger, keyboard)) || (gamepad.code != 0 && Same(a_trigger, gamepad));
		}

		// The ini key that opens the quick picker without waiting for the cast
		// animation. It is a binding of its own: by default it names the same key the
		// game uses, but it may also name a controller button the game does not bind
		// to Shout - the only way a controller user gets the short path, and the
		// reason the ini text accepts "pad:lb" here as well.
		Chord QuickslotKeyChord()
		{
			std::lock_guard lock(g_chordMutex);
			return g_quickKey;
		}

		bool QuickslotKeyHeld()
		{
			const auto chord = QuickslotKeyChord();
			return chord.valid() && AllDown(chord);
		}

		bool IsQuickslotKey(const Trigger& a_trigger)
		{
			const auto chord = QuickslotKeyChord();
			if (!chord.valid() || !Contains(chord.triggers, a_trigger)) {
				return false;
			}
			return AllDown(chord);
		}

		// Any key that may open the quick picker: the game's own Shout/Power binding
		// on either device, or the ini key.
		bool IsPowerKey(const Trigger& a_trigger)
		{
			return IsGamePowerKey(a_trigger) || IsQuickslotKey(a_trigger);
		}

		bool PowerKeyHeld()
		{
			Trigger keyboard{};
			Trigger gamepad{};
			if (GamePowerKeys(keyboard, gamepad)) {
				if ((keyboard.code != 0 && IsHeld(keyboard)) || (gamepad.code != 0 && IsHeld(gamepad))) {
					return true;
				}
			}
			return QuickslotKeyHeld();
		}

		// One press of the game's own power key, handled on the press itself. The
		// caller consumes the event, so the game never casts the power and there is
		// no cast animation left to feel like a cooldown.
		//
		// The return value says whether the event was handled, i.e. whether the
		// caller has to take it out of the queue.
		//
		// Either key only belongs to this plugin while the Change Outfit power is
		// the one the player has selected. The ini key names the very key the game
		// binds to Shout by default, so answering it while another power is equipped
		// would open the window instead of using that power - the key is passed to
		// the game untouched in that case, and none of the other powers is affected.
		bool HandlePowerPress(const Trigger& a_trigger, bool a_blocked)
		{
			if (!IsPowerKey(a_trigger)) {
				return false;
			}
			if (!OurPowerSelected()) {
				return false;
			}
			// While a menu owns the input the key is not ours: swallowing it there
			// would break the menu.
			if (a_blocked) {
				return false;
			}
			if (g_latchedQuickKey) {
				return true;
			}
			g_latchedQuickKey = true;
			const auto now = NowMs();
			const auto last = g_lastQuickslotOpenMs.load(std::memory_order_relaxed);
			if (last != 0 && now - last < kQuickslotOpenCooldownMs) {
				return true;
			}
			g_lastQuickslotOpenMs.store(now, std::memory_order_relaxed);
			FireQuickslotKey();
			return true;
		}

		// Polled variant for the case where the front run of the power key never
		// produced an event (for example when the power was swapped while the key was
		// held). The same rule as above applies: only while the Change Outfit power is
		// the selected one.
		void CheckPowerKey(bool a_blocked)
		{
			if (!PowerKeyHeld()) {
				g_latchedQuickKey = false;
				return;
			}
			if (!OurPowerSelected()) {
				g_latchedQuickKey = false;
				return;
			}
			if (g_latchedQuickKey || a_blocked) {
				return;
			}
			g_latchedQuickKey = true;
			const auto now = NowMs();
			const auto last = g_lastQuickslotOpenMs.load(std::memory_order_relaxed);
			if (last != 0 && now - last < kQuickslotOpenCooldownMs) {
				return;
			}
			g_lastQuickslotOpenMs.store(now, std::memory_order_relaxed);
			FireQuickslotKey();
		}

		// Fires a hotkey once per press. The latch is cleared as soon as the chord is
		// not fully held any more, so keeping the keys down does not repeat it.
		void CheckChords(bool a_blocked)
		{
			Chord randomKb;
			Chord randomPad;
			Chord quickKb;
			Chord quickPad;
			{
				std::lock_guard lock(g_chordMutex);
				randomKb = g_randomKb;
				randomPad = g_randomPad;
				quickKb = g_quickKb;
				quickPad = g_quickPad;
			}

			if (AnyChordBound(randomKb, randomPad)) {
				if (!AnyChordDown(randomKb, randomPad)) {
					g_latchedRandom = false;
				} else if (!g_latchedRandom && !a_blocked) {
					g_latchedRandom = true;
					SKSE::log::debug("Hotkeys: the random outfit hotkey fired.");
					FireRandomThrottled();
				}
			}

			if (AnyChordBound(quickKb, quickPad)) {
				if (!AnyChordDown(quickKb, quickPad)) {
					g_latchedQuick = false;
				} else if (!g_latchedQuick && !a_blocked) {
					g_latchedQuick = true;
					SKSE::log::debug("Hotkeys: the quick picker hotkey fired.");
					FireQuickPicker();
				}
			}
		}

		void PollTick()
		{
			// Firing is driven by the input sink; while the recorder is open this is its
			// sampling and its heartbeat, and the settings page calls it every frame.
			if (!g_capturing.load(std::memory_order_relaxed)) {
				return;
			}
			PollCapture();
		}

		class InputSink : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				// Input events are the cheapest heartbeat we get: use them to re-assert
				// the compatibility state that other mods overwrite.
				Compat::Tick();
				// Weather changes have no event of their own; poll the state a couple of
				// times per second while the player is playing.
				AutoSwitch::Tick();
				MorphRefresh::Tick();

				if (!a_event) {
					return RE::BSEventNotifyControl::kContinue;
				}

				bool       swallow = false;
				const bool blocked = FiringBlocked();

				for (auto* event = *a_event; event; event = event->next) {
					if (g_capturing.load(std::memory_order_relaxed)) {
						// The recorder owns the input while it is open: the snapshot it works on is
						// polled (see PollDevices), so nothing has to be collected here - the events
						// are only kept away from the rest of the game. The events themselves are
						// taken off the queue by the device hooks (see InputQueueFilter.cpp), which
						// is what keeps Escape, Back and the shoulder buttons off the menu, and the
						// menu framework is told to stop reading input as well. Escape and Back end
						// the recording.
						if (EventAsksCancel(event)) {
							CancelCapture();
						}
						swallow = true;
						continue;
					}
					auto* button = event->AsButtonEvent();
					if (!button) {
						continue;
					}
					const auto device = button->GetDevice();
					if (device != RE::INPUT_DEVICE::kKeyboard && device != RE::INPUT_DEVICE::kGamepad) {
						continue;
					}

					Trigger trigger;
					trigger.device = device == RE::INPUT_DEVICE::kGamepad ? Dev::kGamepad : Dev::kKeyboard;
					trigger.code = button->GetIDCode();
					if (trigger.device == Dev::kKeyboard) {
						const auto modifier = ModifierForScan(trigger.code);
						if (modifier != 0) {
							trigger.code = modifier;
						}
					}

					const bool down = button->IsDown();
					const bool up = button->IsUp();
					if (!down && !up) {
						continue;
					}

					Remember(trigger, down);

					if (down && IsPowerKey(trigger)) {
						// The game's own Shout/Power key, or the ini key that skips the
						// cast animation. It is handled here, on the press, and the event
						// is consumed: letting the game cast the power as well is what
						// used to add the cast animation on top of our own timer, which
						// felt like a cooldown that never went away. With another power
						// equipped (or none) the key is passed through.
						if (HandlePowerPress(trigger, blocked)) {
							swallow = true;
						}
					}

					CheckChords(blocked);

					if (!down) {
						if (IsPowerKey(trigger)) {
							g_latchedQuickKey = false;
						}
						continue;
					}

					CheckPowerKey(blocked);
				}
				return swallow ? RE::BSEventNotifyControl::kStop : RE::BSEventNotifyControl::kContinue;
			}
		};

		InputSink g_sink;
	}

	void Tick()
	{
		PollTick();
	}

	void NotifyMenuState(bool a_open)
	{
		g_frameworkMenuOpen.store(a_open, std::memory_order_relaxed);
		// Nothing that was held while the menu owned the input may survive into the
		// gameplay: the key-up events were eaten by the menu.
		ForgetAll();
		g_latchedRandom = false;
		g_latchedQuick = false;
		g_latchedQuickKey = false;
		SKSE::log::debug("Hotkeys: the framework menu is {}; the hotkeys are {}.", a_open ? "open" : "closed", a_open ? "paused" : "live");
	}

	std::string GamePowerKeyText()
	{
		Trigger keyboard{};
		Trigger gamepad{};
		if (!GamePowerKeys(keyboard, gamepad)) {
			return {};
		}
		Chord chord;
		if (keyboard.code != 0) {
			chord.triggers.push_back(keyboard);
		}
		if (gamepad.code != 0) {
			chord.triggers.push_back(gamepad);
		}
		return Format(chord);
	}

	bool PowerActionJustHandled()
	{
		const auto now = NowMs();
		const auto last = g_lastPowerActionMs.load(std::memory_order_relaxed);
		return last != 0 && now - static_cast<std::int64_t>(last) < 1500;
	}

	bool Install()
	{
		if (g_installed) {
			return true;
		}

		auto* manager = RE::BSInputDeviceManager::GetSingleton();
		if (!manager) {
			SKSE::log::error("Hotkeys: the input device manager is not available; hotkeys disabled.");
			return false;
		}

		Reload();
		manager->AddEventSink<RE::InputEvent*>(&g_sink);
		// The recorder takes the player's keys off the input queue while it runs, the
		// way FreeClimb does; a slot that cannot be hooked is only a warning.
		InstallInputFilters();
		g_installed = true;
		SKSE::log::debug("Hotkeys: input sink registered (random '{}', quick picker '{}', quickslot key '{}').",
			Settings::Get().randomOutfitHotkey, Settings::Get().quickPickerHotkey, Settings::Get().quickPickerKey);
		return true;
	}

	void Reload()
	{
		const auto& settings = Settings::Get();
		const auto  randomAll = Parse(settings.randomOutfitHotkey, "random outfit");
		const auto  randomPad = Parse(settings.randomOutfitGamepadHotkey, "random outfit (controller)");
		const auto  quickAll = Parse(settings.quickPickerHotkey, "quick picker");
		const auto  quickPad = Parse(settings.quickPickerGamepadHotkey, "quick picker (controller)");
		const auto  quickKey = Parse(settings.quickPickerKey, "quickslot");

		// An ini written by an older build kept both devices in one value. The
		// keyboard half stays where it is and the controller half moves into its own
		// field, so an upgrade never drops a binding.
		const auto randomKb = OnlyDevice(randomAll, Dev::kKeyboard);
		const auto quickKb = OnlyDevice(quickAll, Dev::kKeyboard);
		auto       padRandom = randomPad;
		auto       padQuick = quickPad;
		AppendChord(padRandom, OnlyDevice(randomAll, Dev::kGamepad));
		AppendChord(padQuick, OnlyDevice(quickAll, Dev::kGamepad));
		{
			std::lock_guard lock(g_chordMutex);
			g_randomKb = randomKb;
			g_randomPad = padRandom;
			g_quickKb = quickKb;
			g_quickPad = padQuick;
			g_quickKey = quickKey;
		}
		ForgetAll();
		g_latchedRandom = false;
		g_latchedQuick = false;
		g_latchedQuickKey = false;
		g_lastQuickslotOpenMs.store(0, std::memory_order_relaxed);
		if (randomKb.valid() || padRandom.valid()) {
			SKSE::log::debug("Hotkeys: random outfit hotkey keyboard '{}', controller '{}'.", Format(randomKb), Format(padRandom));
		} else {
			SKSE::log::debug("Hotkeys: random outfit hotkey is disabled.");
		}
		if (quickKb.valid() || padQuick.valid()) {
			SKSE::log::debug("Hotkeys: quick picker hotkey keyboard '{}', controller '{}'.", Format(quickKb), Format(padQuick));
		} else {
			SKSE::log::debug("Hotkeys: quick picker hotkey is disabled.");
		}
		if (quickKey.valid()) {
			SKSE::log::debug("Hotkeys: quickslot key '{}' opens the quick picker right away while the Change Outfit power is selected (the power needs its cast animation).", settings.quickPickerKey);
		} else {
			SKSE::log::debug("Hotkeys: the quickslot quick picker key is disabled.");
		}
	}

	void SetTextInputActive(bool a_active)
	{
		const auto now = NowMs();
		g_textInputStamp.store(a_active ? now : 0, std::memory_order_relaxed);
		g_textInput.store(a_active, std::memory_order_relaxed);
	}

	void BeginCapture(Slot a_slot, Input a_device)
	{
		const auto wanted = a_device == Input::kGamepad ? Dev::kGamepad : Dev::kKeyboard;
		{
			std::lock_guard lock(g_captureMutex);
			g_captureDevice.store(wanted == Dev::kGamepad ? 1 : 0, std::memory_order_relaxed);
			g_captureCandidate.clear();
			g_captureOrder.clear();
			g_captureResult.clear();
			g_captureResultSlot = -1;
			g_captureResultDevice = 0;
			g_captureProblem.store(static_cast<int>(CaptureProblem::kNone), std::memory_order_relaxed);
			// The recorder always starts by waiting for a neutral device: the click, the
			// button or the key that opened it is still down and must not become part of
			// the chord. Nothing is accepted before that state is reached.
			g_captureState.store(static_cast<int>(CaptureState::kWaiting), std::memory_order_relaxed);
			g_captureStartedMs = NowMs();
		}
		{
			std::lock_guard lock(g_captureMutex);
			g_capture.begin(wanted == Dev::kGamepad ? fc::BindingCaptureDevice::gamepad : fc::BindingCaptureDevice::keyboard);
		}
		g_captureSlot.store(static_cast<int>(a_slot), std::memory_order_relaxed);
		g_capturing.store(true, std::memory_order_relaxed);
		// Whatever is held right now is not part of the new chord.
		ForgetAll();
		// Keys a previous recording took off the queue must not stay marked as ours.
		ResetInputOwnership();
		SKSE::log::debug("Hotkeys: recording the {} hotkey of slot {}; waiting for a neutral state.",
			wanted == Dev::kGamepad ? "controller" : "keyboard", static_cast<int>(a_slot));
	}

	void CancelCapture()
	{
		g_capturing.store(false, std::memory_order_relaxed);
		g_captureSlot.store(-1, std::memory_order_relaxed);
		g_captureState.store(static_cast<int>(CaptureState::kIdle), std::memory_order_relaxed);
		g_captureProblem.store(static_cast<int>(CaptureProblem::kNone), std::memory_order_relaxed);
		{
			std::lock_guard lock(g_captureMutex);
			g_capture.reset();
			g_captureCandidate.clear();
			g_captureOrder.clear();
			g_captureResult.clear();
			g_captureResultSlot = -1;
			g_captureResultDevice = 0;
			g_captureStartedMs = 0;
		}
		ForgetAll();
	}

	void FeedInputEvent(RE::InputEvent* a_event)
	{
		// The recorder works on its own polled snapshot, so this is only where the
		// events the framework hands us are used to recognise Escape and the
		// controller's Back button.
		if (!g_capturing.load(std::memory_order_relaxed)) {
			return;
		}
		for (auto* event = a_event; event; event = event->next) {
			if (EventAsksCancel(event)) {
				CancelCapture();
				return;
			}
		}
	}

	bool CaptureActive()
	{
		return g_capturing.load(std::memory_order_relaxed);
	}

	Input CaptureDevice()
	{
		return g_captureDevice.load(std::memory_order_relaxed) == 0 ? Input::kKeyboard : Input::kGamepad;
	}

	bool CaptureArmed()
	{
		const auto state = static_cast<CaptureState>(g_captureState.load(std::memory_order_relaxed));
		return state == CaptureState::kArmed;
	}

	int CaptureProblem()
	{
		return g_captureProblem.load(std::memory_order_relaxed);
	}

	std::string CaptureText()
	{
		std::lock_guard lock(g_captureMutex);
		if (g_captureCandidate.empty()) {
			return {};
		}
		Chord chord;
		chord.triggers = g_captureCandidate;
		return Format(chord);
	}

	bool TakeCaptureResult(Slot& a_slot, Input& a_device, std::string& a_text)
	{
		std::lock_guard lock(g_captureMutex);
		// This runs every frame while the window is open, so it must not touch the
		// running recorder: resetting it here threw away the whole recording (that is
		// what made 1.5.74 record nothing at all).
		if (static_cast<CaptureState>(g_captureState.load(std::memory_order_relaxed)) != CaptureState::kCaptured || g_captureResultSlot < 0) {
			return false;
		}
		a_slot = static_cast<Slot>(g_captureResultSlot);
		a_device = g_captureResultDevice == 0 ? Input::kKeyboard : Input::kGamepad;
		a_text = g_captureResult;
		g_captureResult.clear();
		g_captureResultSlot = -1;
		g_captureResultDevice = 0;
		g_captureCandidate.clear();
		g_captureOrder.clear();
		g_captureProblem.store(static_cast<int>(CaptureProblem::kNone), std::memory_order_relaxed);
		g_captureState.store(static_cast<int>(CaptureState::kIdle), std::memory_order_relaxed);
		g_captureStartedMs = 0;
		g_capturing.store(false, std::memory_order_relaxed);
		g_captureSlot.store(-1, std::memory_order_relaxed);
		ForgetAll();
		return true;
	}
}
