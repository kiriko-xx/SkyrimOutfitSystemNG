#include "NGHotkeys.h"

#include "I18n.h"

#include "AutoSwitch.h"
#include "MenuBridge.h"
#include "MenuUI.h"
#include "MorphRefresh.h"
#include "SceneOutfits.h"
#include "compat/ModCompat.h"
#include "core/Settings.h"

#include <algorithm>
#include <atomic>
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

		bool Down(const Trigger& a_trigger)
		{
			if (a_trigger.device == Dev::kKeyboard && a_trigger.code >= kModCtrl) {
				int vk = 0;
				if (a_trigger.code == kModCtrl) {
					vk = VK_CONTROL;
				} else if (a_trigger.code == kModShift) {
					vk = VK_SHIFT;
				} else {
					vk = VK_MENU;
				}
				return (::GetAsyncKeyState(vk) & 0x8000) != 0;
			}
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
		// Modelled on FreeClimb's recorder: the recorder is a modal window, it
		// accepts nothing until the player released whatever was held when it
		// opened, it says why a combination was refused, and it gives up after a
		// while. Unlike FreeClimb the combination is only finished once no new key
		// was pressed for a moment, so a chord pressed one key after the other is
		// still recorded as a chord.

		enum class CaptureState : int
		{
			kIdle = 0,
			kWaiting,
			kArmed,
			kSettling,
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
		std::vector<Trigger> g_captureDown;
		std::string          g_captureResult;
		int                  g_captureResultSlot{ -1 };
		int                  g_captureResultDevice{ 0 };
		std::int64_t         g_captureStartedMs{ 0 };
		std::int64_t         g_captureQuietMs{ 0 };
		// A recording that is left open (the player walked away) must not keep the
		// host from reading input for the rest of the session.
		constexpr std::int64_t kCaptureTimeoutMs = 20000;
		// Keys pressed one after the other belong to the same chord as long as the
		// next one follows within this window.
		constexpr std::int64_t kCaptureSettleMs = 400;

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

		// Refuses the combinations the game cannot use, so a refused recording tells
		// the user instead of silently storing a hotkey that never fires.
		bool ValidCaptureChord(Dev a_device, const std::vector<Trigger>& a_triggers)
		{
			if (a_triggers.empty() || a_triggers.size() > kMaxChordKeys) {
				return false;
			}
			bool ctrl = false;
			bool alt = false;
			bool tab = false;
			bool f4 = false;
			bool del = false;
			for (const auto& trigger : a_triggers) {
				if (trigger.device != a_device) {
					return false;
				}
				if (a_device == Dev::kGamepad) {
					using Keys = RE::BSWin32GamepadDevice::Keys;
					// Back cancels a recording and Start belongs to the game menu.
					if (trigger.code == Keys::kBack || trigger.code == Keys::kStart) {
						return false;
					}
					continue;
				}
				if (trigger.code == 0x01) {  // Esc cancels a recording
					return false;
				}
				ctrl = ctrl || trigger.code == kModCtrl;
				alt = alt || trigger.code == kModAlt;
				tab = tab || trigger.code == 0x0F;
				f4 = f4 || trigger.code == 0x3E;
				del = del || trigger.code == 0xD3;
			}
			if (a_device == Dev::kGamepad) {
				return true;
			}
			// The combinations the game or Windows keeps for itself.
			if (alt && (tab || f4)) {
				return false;
			}
			return !(ctrl && alt && del);
		}

		void FailCaptureLocked(CaptureProblem a_problem)
		{
			g_captureProblem.store(static_cast<int>(a_problem), std::memory_order_relaxed);
			g_captureCandidate.clear();
			// Listen again once everything is released, so a refused chord can be
			// retried without closing the window.
			g_captureState.store(static_cast<int>(CaptureState::kWaiting), std::memory_order_relaxed);
			g_captureQuietMs = NowMs();
		}

		void FinishCaptureLocked()
		{
			Chord chord;
			chord.triggers = g_captureCandidate;
			g_captureResult = Format(chord);
			g_captureResultSlot = g_captureSlot.load(std::memory_order_relaxed);
			g_captureResultDevice = g_captureDevice.load(std::memory_order_relaxed);
			g_captureState.store(static_cast<int>(CaptureState::kCaptured), std::memory_order_relaxed);
			SKSE::log::debug("Hotkeys: recorded '{}' for the {} device.", g_captureResult, g_captureResultDevice == 0 ? "keyboard" : "controller");
		}

		// Called after every change of g_captureDown. Keys that are pressed while the
		// recorder listens are added to the combination; the combination is finished
		// once nothing new arrives for kCaptureSettleMs and every key is released
		// again.
		void AdvanceCaptureLocked()
		{
			const auto state = static_cast<CaptureState>(g_captureState.load(std::memory_order_relaxed));
			if (state != CaptureState::kWaiting && state != CaptureState::kArmed && state != CaptureState::kSettling) {
				return;
			}
			if (g_captureDown.empty()) {
				if (state == CaptureState::kWaiting) {
					g_captureState.store(static_cast<int>(CaptureState::kArmed), std::memory_order_relaxed);
					g_captureQuietMs = NowMs();
					return;
				}
				if (state == CaptureState::kArmed && !g_captureCandidate.empty()) {
					g_captureState.store(static_cast<int>(CaptureState::kSettling), std::memory_order_relaxed);
					g_captureQuietMs = NowMs();
				}
				return;
			}
			if (state == CaptureState::kWaiting) {
				return;  // wait for a neutral state first
			}

			bool added = false;
			for (const auto& trigger : g_captureDown) {
				if (!Contains(g_captureCandidate, trigger)) {
					g_captureCandidate.push_back(trigger);
					added = true;
				}
			}
			if (!added) {
				return;
			}
			g_captureQuietMs = NowMs();
			if (g_captureCandidate.size() > kMaxChordKeys) {
				FailCaptureLocked(CaptureProblem::kTooMany);
				return;
			}
			if (!ValidCaptureChord(CaptureDeviceDev(), g_captureCandidate)) {
				FailCaptureLocked(CaptureProblem::kInvalid);
				return;
			}
			g_captureProblem.store(static_cast<int>(CaptureProblem::kNone), std::memory_order_relaxed);
			g_captureState.store(static_cast<int>(CaptureState::kArmed), std::memory_order_relaxed);
		}

		void FeedCapture(const Trigger& a_trigger, bool a_down)
		{
			// A recording that was started for one device ignores the other one, so a
			// palm on the controller cannot put a button into a keyboard chord (and the
			// keyboard cannot pollute a controller chord).
			if (a_trigger.device != CaptureDeviceDev()) {
				return;
			}
			if (a_down && IsCaptureCancel(a_trigger)) {
				return;  // the caller ends the recording
			}
			std::lock_guard lock(g_captureMutex);
			if (a_down) {
				if (!Contains(g_captureDown, a_trigger)) {
					g_captureDown.push_back(a_trigger);
				}
			} else {
				g_captureDown.erase(std::remove_if(g_captureDown.begin(), g_captureDown.end(), [&](const Trigger& t) { return Same(t, a_trigger); }), g_captureDown.end());
			}
			AdvanceCaptureLocked();
		}

		// Runs from the render thread every frame while the recorder is open.
		void CheckCaptureSettle()
		{
			if (!g_capturing.load(std::memory_order_relaxed)) {
				return;
			}
			std::lock_guard lock(g_captureMutex);
			const auto state = static_cast<CaptureState>(g_captureState.load(std::memory_order_relaxed));
			if (state != CaptureState::kSettling || !g_captureDown.empty()) {
				return;
			}
			if (NowMs() - g_captureQuietMs < kCaptureSettleMs) {
				return;
			}
			FinishCaptureLocked();
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
			std::uint32_t buttons;
			std::uint8_t  leftTrigger;
			std::uint8_t  rightTrigger;
			std::uint8_t  rest[10];
		};

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
			PadState state{};
			if (getState(0, &state) != 0) {
				return false;
			}
			a_state = state;
			return true;
		}

		void PollDevices(std::vector<Trigger>& a_now)
		{
			for (const auto& entry : KeyTable()) {
				if ((::GetAsyncKeyState(entry.vk) & 0x8000) != 0) {
					a_now.push_back(Trigger{ Dev::kKeyboard, entry.scan });
				}
			}

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
			if (!lastPadValid) {
				return;
			}
			static constexpr std::uint32_t kPadButtons[] = {
				0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080,
				0x0100, 0x0200, 0x1000, 0x2000, 0x4000, 0x8000
			};
			for (const auto button : kPadButtons) {
				if ((pad.buttons & button) != 0) {
					a_now.push_back(Trigger{ Dev::kGamepad, button });
				}
			}
			if (pad.leftTrigger > 30) {
				a_now.push_back(Trigger{ Dev::kGamepad, RE::BSWin32GamepadDevice::Keys::kLeftTrigger });
			}
			if (pad.rightTrigger > 30) {
				a_now.push_back(Trigger{ Dev::kGamepad, RE::BSWin32GamepadDevice::Keys::kRightTrigger });
			}
		}

		bool IsHeld(const Trigger& a_trigger)
		{
			std::lock_guard lock(g_heldMutex);
			return Contains(g_held, a_trigger);
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
						const auto mask = SKSE::InputMap::GamepadKeycodeToMask(gamepad);
						if (mask != 0 && mask != 0xFF) {
							cachedGamepad = mask;
						}
					}
				}
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

		// True while the "Change Outfit" lesser power is the one the player has
		// selected in the magic menu. Pressing the power key is ours to handle only
		// then: with no power selected, or with any other power equipped, the key
		// belongs to the game and must be passed through untouched.
		bool OurPowerSelected()
		{
			static RE::TESForm* cached = nullptr;
			if (!cached) {
				cached = RE::TESForm::LookupByEditorID<RE::SpellItem>("SkyrimOutfitSystemQuickslotSpell");
				if (!cached) {
					if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
						cached = dataHandler->LookupForm<RE::SpellItem>(0x000182F, "SkyrimOutfitSystem.esp");
					}
				}
				if (cached) {
					SKSE::log::debug("Hotkeys: the Change Outfit power is {:08X}.", cached->GetFormID());
				}
			}
			if (!cached) {
				return false;
			}
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return false;
			}
			return player->GetActorRuntimeData().selectedPower == cached;
		}

		// True when the trigger is one of the keys the game has bound to Shout/Power,
		// keyboard or controller.
		bool IsPowerKey(const Trigger& a_trigger)
		{
			Trigger keyboard{};
			Trigger gamepad{};
			if (!GamePowerKeys(keyboard, gamepad)) {
				return false;
			}
			return (keyboard.code != 0 && Same(a_trigger, keyboard)) || (gamepad.code != 0 && Same(a_trigger, gamepad));
		}

		// One press of the game's own power key, handled on the press itself. The
		// caller consumes the event, so the game never casts the power and there is
		// no cast animation left to feel like a cooldown.
		bool HandlePowerPress(bool a_blocked)
		{
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
		// held).
		void CheckPowerKey(bool a_blocked)
		{
			if (!OurPowerSelected()) {
				g_latchedQuickKey = false;
				return;
			}
			Trigger keyboard{};
			Trigger gamepad{};
			if (!GamePowerKeys(keyboard, gamepad)) {
				g_latchedQuickKey = false;
				return;
			}
			const bool down = (keyboard.code != 0 && IsHeld(keyboard)) || (gamepad.code != 0 && IsHeld(gamepad));
			if (!down) {
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
			static std::vector<Trigger> last;

			const bool                capturing = g_capturing.load(std::memory_order_relaxed);
			std::vector<Trigger>      now;
			PollDevices(now);

			if (!capturing) {
				// Nothing to do: firing is driven by the input sink, and while the page
				// is open the keys belong to the menu.
				last.clear();
				return;
			}

			const std::vector<Trigger> previous = last;
			for (const auto& trigger : now) {
				if (!Contains(previous, trigger)) {
					FeedCapture(trigger, true);
				}
			}
			for (const auto& trigger : previous) {
				if (!Contains(now, trigger)) {
					FeedCapture(trigger, false);
				}
			}
			last = now;

			// Only the moment the cancel key goes down ends the recording, so holding
			// it does not kill the next attempt.
			for (const auto& trigger : now) {
				if (!Contains(previous, trigger) && trigger.device == CaptureDeviceDev() && IsCaptureCancel(trigger)) {
					CancelCapture();
					last.clear();
					return;
				}
			}
			CheckCaptureSettle();
			CheckCaptureTimeout();
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

				const bool capturing = g_capturing.load(std::memory_order_relaxed);
				bool       swallow = false;
				const bool blocked = FiringBlocked();

				for (auto* event = *a_event; event; event = event->next) {
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

					if (capturing) {
						// The recorder is modal. While it is open every button event is taken
						// out of the queue, so the framework menu cannot navigate or activate
						// the entry that opened the recorder with the very keys being
						// recorded (a pad "A" used to both record and confirm, which is also
						// why chords never came out longer than one button).
						if (down && trigger.device == CaptureDeviceDev() && IsCaptureCancel(trigger)) {
							CancelCapture();
							break;
						}
						FeedCapture(trigger, down);
						swallow = true;
						continue;
					}

					Remember(trigger, down);

					if (down && IsPowerKey(trigger)) {
						// The game's own Shout/Power key. It is handled here, on the press,
						// and the event is consumed: letting the game cast the power as well
						// is what used to add the cast animation on top of our own timer,
						// which felt like a cooldown that never went away. With another power
						// equipped (or none) the key is passed through.
						if (HandlePowerPress(blocked)) {
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
			SKSE::log::debug("Hotkeys: quickslot key '{}' opens the quick picker right away (the power needs its cast animation).", settings.quickPickerKey);
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
		// Snapshot the devices first: whatever is held right now (usually the button
		// the player used to open the recorder) has to be waited out instead of
		// becoming part of the chord.
		std::vector<Trigger> held;
		PollDevices(held);
		const auto wanted = a_device == Input::kGamepad ? Dev::kGamepad : Dev::kKeyboard;
		{
			std::lock_guard lock(g_captureMutex);
			g_captureDevice.store(wanted == Dev::kGamepad ? 1 : 0, std::memory_order_relaxed);
			g_captureCandidate.clear();
			g_captureDown.clear();
			for (const auto& trigger : held) {
				if (trigger.device == wanted) {
					g_captureDown.push_back(trigger);
				}
			}
			g_captureResult.clear();
			g_captureResultSlot = -1;
			g_captureResultDevice = 0;
			g_captureProblem.store(static_cast<int>(CaptureProblem::kNone), std::memory_order_relaxed);
			g_captureState.store(static_cast<int>(CaptureState::kWaiting), std::memory_order_relaxed);
			g_captureStartedMs = NowMs();
			g_captureQuietMs = g_captureStartedMs;
		}
		g_captureSlot.store(static_cast<int>(a_slot), std::memory_order_relaxed);
		g_capturing.store(true, std::memory_order_relaxed);
		// Whatever is held right now is not part of the new chord.
		ForgetAll();
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
			g_captureCandidate.clear();
			g_captureDown.clear();
			g_captureResult.clear();
			g_captureResultSlot = -1;
			g_captureResultDevice = 0;
			g_captureStartedMs = 0;
			g_captureQuietMs = 0;
		}
		ForgetAll();
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
		return state == CaptureState::kArmed || state == CaptureState::kSettling;
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
		g_captureDown.clear();
		g_captureProblem.store(static_cast<int>(CaptureProblem::kNone), std::memory_order_relaxed);
		g_captureState.store(static_cast<int>(CaptureState::kIdle), std::memory_order_relaxed);
		g_captureStartedMs = 0;
		g_captureQuietMs = 0;
		g_capturing.store(false, std::memory_order_relaxed);
		g_captureSlot.store(-1, std::memory_order_relaxed);
		ForgetAll();
		return true;
	}
}
