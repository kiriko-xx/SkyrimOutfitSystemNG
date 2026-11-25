#include "NGHotkeys.h"

#include "I18n.h"

#include "AutoSwitch.h"
#include "MorphRefresh.h"
#include "SceneOutfits.h"

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
#include "MenuUI.h"
#include "compat/ModCompat.h"
#include "core/Settings.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <Windows.h>

namespace SOS::Hotkeys
{
	namespace
	{
		// A hotkey is a chord: every trigger has to be down at the same time.
		// Keyboard triggers use DirectInput scan codes (what ButtonEvent reports);
		// the three modifier keys are kept as pseudo codes so that the ini text
		// stays readable ("ctrl", not "lctrl") and the old ini values keep
		// working.
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

		Chord      g_random;
		Chord      g_quick;
		Chord      g_quickKey;
		std::mutex g_chordMutex;

		// Everything currently held down, in press order. Maintained from the
		// input sink, which runs on the device thread.
		std::vector<Trigger> g_held;
		std::mutex          g_heldMutex;

		bool g_latchedRandom{ false };
		bool g_latchedQuick{ false };
		bool g_latchedQuickKey{ false };

		bool       g_installed{ false };
		std::atomic<bool> g_textInput{ false };
		std::atomic<std::int64_t> g_textInputStamp{ 0 };

		std::atomic<std::int64_t> g_lastQuickslotOpenMs{ 0 };

		// The quickslot power only opens the window once its cast animation has
		// finished, which feels like a cooldown on the Z key. The mirrored key
		// skips that wait and is limited to one open every 0.5 s.
		constexpr std::int64_t kQuickslotOpenCooldownMs = 500;

		bool CachedTextInputActive()
		{
			if (!g_textInput.load(std::memory_order_relaxed)) {
				return false;
			}
			// The menu refreshes this timestamp from the render thread every frame.
			// Expiring it avoids leaving hotkeys blocked when the menu closes while
			// an input box still had focus on the last rendered frame.
			const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now().time_since_epoch()).count();
			const auto stamp = g_textInputStamp.load(std::memory_order_relaxed);
			return stamp != 0 && now - stamp < 500;
		}

		std::int64_t NowMs()
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now().time_since_epoch()).count();
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

		// DirectInput scan codes, as reported by ButtonEvent::GetIDCode().
		std::uint32_t ScanCodeFor(std::string_view a_token)
		{
			if (a_token.size() == 1) {
				const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(a_token.front())));
				if (c >= 'A' && c <= 'Z') {
					static constexpr std::uint32_t kLetters[26] = {
						0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
						0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C
					};
					return kLetters[c - 'A'];
				}
				if (c >= '0' && c <= '9') {
					static constexpr std::uint32_t kDigits[10] = {
						0x0B, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A
					};
					return kDigits[c - '0'];
				}
			}
			if (a_token.size() >= 2 && a_token.front() == 'F') {
				std::uint32_t index = 0;
				bool          digits = true;
				for (std::size_t i = 1; i < a_token.size(); ++i) {
					if (!std::isdigit(static_cast<unsigned char>(a_token[i]))) {
						digits = false;
						break;
					}
					index = index * 10 + static_cast<std::uint32_t>(a_token[i] - '0');
				}
				if (digits) {
					if (index >= 1 && index <= 10) {
						return 0x3A + index;  // F1 = 0x3B
					}
					if (index == 11) {
						return 0x57;
					}
					if (index == 12) {
						return 0x58;
					}
				}
			}
			if (a_token == "space") {
				return 0x39;
			}
			if (a_token == "tab") {
				return 0x0F;
			}
			if (a_token == "enter" || a_token == "return") {
				return 0x1C;
			}
			if (a_token == "esc" || a_token == "escape") {
				return 0x01;
			}
			if (a_token == "insert") {
				return 0xD2;
			}
			if (a_token == "delete") {
				return 0xD3;
			}
			if (a_token == "home") {
				return 0xC7;
			}
			if (a_token == "end") {
				return 0xCF;
			}
			if (a_token == "pgup") {
				return 0xC9;
			}
			if (a_token == "pgdn") {
				return 0xD1;
			}
			return 0;
		}

		std::string_view NameForScan(std::uint32_t a_code)
		{
			static constexpr std::string_view kLetters[26] = {
				"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
				"N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"
			};
			static constexpr std::uint32_t kLetterCodes[26] = {
				0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
				0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C
			};
			for (std::size_t i = 0; i < 26; ++i) {
				if (kLetterCodes[i] == a_code) {
					return kLetters[i];
				}
			}
			static constexpr std::uint32_t kDigitCodes[10] = { 0x0B, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
			static constexpr std::string_view kDigitNames[10] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };
			for (std::size_t i = 0; i < 10; ++i) {
				if (kDigitCodes[i] == a_code) {
					return kDigitNames[i];
				}
			}
			if (a_code >= 0x3B && a_code <= 0x44) {
				static constexpr std::string_view kF[10] = { "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10" };
				return kF[a_code - 0x3B];
			}
			switch (a_code) {
			case 0x57: return "F11";
			case 0x58: return "F12";
			case 0x39: return "space";
			case 0x0F: return "tab";
			case 0x1C: return "enter";
			case 0x01: return "esc";
			case 0xD2: return "insert";
			case 0xD3: return "delete";
			case 0xC7: return "home";
			case 0xCF: return "end";
			case 0xC9: return "pgup";
			case 0xD1: return "pgdn";
			default: return {};
			}
		}

		// Controller buttons, as reported by ButtonEvent::GetIDCode() on a
		// gamepad device (XInput masks plus the two trigger ids).
		std::uint32_t GamepadCodeFor(std::string_view a_token)
		{
			using Keys = RE::BSWin32GamepadDevice::Keys;
			if (a_token == "up") return Keys::kUp;
			if (a_token == "down") return Keys::kDown;
			if (a_token == "left") return Keys::kLeft;
			if (a_token == "right") return Keys::kRight;
			if (a_token == "start") return Keys::kStart;
			if (a_token == "back") return Keys::kBack;
			if (a_token == "lstick") return Keys::kLeftThumb;
			if (a_token == "rstick") return Keys::kRightThumb;
			if (a_token == "lb") return Keys::kLeftShoulder;
			if (a_token == "rb") return Keys::kRightShoulder;
			if (a_token == "lt") return Keys::kLeftTrigger;
			if (a_token == "rt") return Keys::kRightTrigger;
			if (a_token == "a") return Keys::kA;
			if (a_token == "b") return Keys::kB;
			if (a_token == "x") return Keys::kX;
			if (a_token == "y") return Keys::kY;
			return 0;
		}

		std::string_view GamepadNameFor(std::uint32_t a_code)
		{
			using Keys = RE::BSWin32GamepadDevice::Keys;
			switch (a_code) {
			case Keys::kUp: return "up";
			case Keys::kDown: return "down";
			case Keys::kLeft: return "left";
			case Keys::kRight: return "right";
			case Keys::kStart: return "start";
			case Keys::kBack: return "back";
			case Keys::kLeftThumb: return "lstick";
			case Keys::kRightThumb: return "rstick";
			case Keys::kLeftShoulder: return "lb";
			case Keys::kRightShoulder: return "rb";
			case Keys::kLeftTrigger: return "lt";
			case Keys::kRightTrigger: return "rt";
			case Keys::kA: return "a";
			case Keys::kB: return "b";
			case Keys::kX: return "x";
			case Keys::kY: return "y";
			default: return {};
			}
		}

		// The modifier keys have dedicated scan codes; fold the left and the right
		// one onto the same pseudo trigger so the ini text stays identical.
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
					text += "pad:";
					text += GamepadNameFor(trigger.code);
				} else if (trigger.code == kModCtrl) {
					text += "ctrl";
				} else if (trigger.code == kModShift) {
					text += "shift";
				} else if (trigger.code == kModAlt) {
					text += "alt";
				} else {
					const auto name = NameForScan(trigger.code);
					if (name.empty()) {
						text += "?";
					} else {
						text += name;
					}
				}
			}
			return text;
		}

		Chord Parse(const std::string& a_text, const char* a_name)
		{
			Chord      result;
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
				const auto lower = Lower(token, storage);
				Trigger    trigger;
				if (lower == "ctrl" || lower == "control") {
					trigger.code = kModCtrl;
				} else if (lower == "shift") {
					trigger.code = kModShift;
				} else if (lower == "alt") {
					trigger.code = kModAlt;
				} else if (lower.rfind("pad:", 0) == 0) {
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
		// cooldown. This runs on the key press itself; the power then casts as
		// usual and its OpenQuickPicker() call does nothing because the window is
		// already open.
		void FireQuickslotKey()
		{
			auto* tasks = SKSE::GetTaskInterface();
			if (!tasks) {
				return;
			}
			tasks->AddTask([]() {
				const auto& settings = Settings::Get();
				if (settings.quickslotRandom) {
					// That mode rolls a random outfit through the power itself; do
					// nothing here so nothing is rolled twice.
					return;
				}
				if (Menu::QuickPickerVisible()) {
					return;
				}
				Menu::ShowQuickPicker(true);
				SKSE::log::debug("Hotkeys: quick picker opened by the quickslot key chord.");
			});
		}

		// ---- capture ----------------------------------------------------

		std::atomic<bool> g_capturing{ false };
		std::atomic<int>  g_captureSlot{ -1 };
		std::mutex        g_captureMutex;
		std::vector<Trigger> g_captureKeys;
		std::vector<Trigger> g_captureDown;
		bool        g_captureDone{ false };
		bool        g_captureOverflow{ false };
		std::string g_captureResult;
		int         g_captureResultSlot{ -1 };

		void FeedCapture(const Trigger& a_trigger, bool a_down)
		{
			std::lock_guard lock(g_captureMutex);
			if (a_down) {
				if (!Contains(g_captureDown, a_trigger)) {
					g_captureDown.push_back(a_trigger);
				}
				if (!Contains(g_captureKeys, a_trigger)) {
					if (g_captureKeys.size() < kMaxChordKeys) {
						g_captureKeys.push_back(a_trigger);
					} else {
						g_captureOverflow = true;
					}
				}
				return;
			}

			g_captureDown.erase(std::remove_if(g_captureDown.begin(), g_captureDown.end(), [&](const Trigger& t) { return Same(t, a_trigger); }), g_captureDown.end());
			if (g_captureDown.empty() && !g_captureKeys.empty()) {
				Chord chord;
				chord.triggers = g_captureKeys;
				g_captureResult = Format(chord);
				g_captureResultSlot = g_captureSlot.load(std::memory_order_relaxed);
				g_captureDone = true;
			}
		}

		class InputSink : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				// Input events are the cheapest heartbeat we get: use them to
				// re-assert the compatibility state that other mods overwrite.
				Compat::Tick();
				// Weather changes have no event of their own; poll the state a
				// couple of times per second while the player is playing.
				AutoSwitch::Tick();
				MorphRefresh::Tick();

				if (!a_event) {
					return RE::BSEventNotifyControl::kContinue;
				}

				const bool capturing = g_capturing.load(std::memory_order_relaxed);

				const bool inputBlocked = CachedTextInputActive();
				auto*      ui = RE::UI::GetSingleton();
				const bool consoleOpen = ui && ui->IsMenuOpen(RE::Console::MENU_NAME);
				// The mirrored quickslot key must not fire from a paused menu
				// (inventory, map, container) where the power itself cannot be
				// cast.
				const bool menuOpen = ui && ui->GameIsPaused();

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
						FeedCapture(trigger, down);
						continue;
					}

					Remember(trigger, down);
					if (!down) {
						continue;
					}

					Chord random;
					Chord quick;
					Chord quickKey;
					{
						std::lock_guard lock(g_chordMutex);
						random = g_random;
						quick = g_quick;
						quickKey = g_quickKey;
					}

					if (random.valid()) {
						if (!AllDown(random)) {
							g_latchedRandom = false;
						} else if (!g_latchedRandom && !inputBlocked && !consoleOpen) {
							g_latchedRandom = true;
							Fire();
						}
					}

					if (quick.valid()) {
						if (!AllDown(quick)) {
							g_latchedQuick = false;
						} else if (!g_latchedQuick && !inputBlocked && !consoleOpen) {
							g_latchedQuick = true;
							FireQuickPicker();
						}
					}

					// The quickslot power chord (default Z): opens the window on the key
					// press instead of after the power's cast animation. The event is
					// not consumed, so the quickslot itself still casts the power.
					if (quickKey.valid()) {
						if (!AllDown(quickKey)) {
							g_latchedQuickKey = false;
						} else if (!g_latchedQuickKey && !inputBlocked && !consoleOpen && !menuOpen) {
							g_latchedQuickKey = true;
							const auto now = NowMs();
							const auto last = g_lastQuickslotOpenMs.load(std::memory_order_relaxed);
							if (!Menu::QuickPickerVisible() && (last == 0 || now - last >= kQuickslotOpenCooldownMs)) {
								g_lastQuickslotOpenMs.store(now, std::memory_order_relaxed);
								FireQuickslotKey();
							}
						}
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		InputSink g_sink;
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
		const auto  random = Parse(settings.randomOutfitHotkey, "random outfit");
		const auto  quick = Parse(settings.quickPickerHotkey, "quick picker");
		const auto  quickKey = Parse(settings.quickPickerKey, "quickslot");
		{
			std::lock_guard lock(g_chordMutex);
			g_random = random;
			g_quick = quick;
			g_quickKey = quickKey;
		}
		ForgetAll();
		g_latchedRandom = false;
		g_latchedQuick = false;
		g_latchedQuickKey = false;
		g_lastQuickslotOpenMs.store(0, std::memory_order_relaxed);
		if (random.valid()) {
			SKSE::log::debug("Hotkeys: random outfit hotkey '{}' -> '{}'.", settings.randomOutfitHotkey, Format(random));
		} else {
			SKSE::log::debug("Hotkeys: random outfit hotkey is disabled.");
		}
		if (quick.valid()) {
			SKSE::log::debug("Hotkeys: quick picker hotkey '{}' -> '{}'.", settings.quickPickerHotkey, Format(quick));
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
		const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
		g_textInputStamp.store(a_active ? now : 0, std::memory_order_relaxed);
		g_textInput.store(a_active, std::memory_order_relaxed);
	}

	void BeginCapture(Slot a_slot)
	{
		{
			std::lock_guard lock(g_captureMutex);
			g_captureKeys.clear();
			g_captureDown.clear();
			g_captureResult.clear();
			g_captureResultSlot = -1;
			g_captureDone = false;
			g_captureOverflow = false;
		}
		g_captureSlot.store(static_cast<int>(a_slot), std::memory_order_relaxed);
		g_capturing.store(true, std::memory_order_relaxed);
		// Whatever is held right now is not part of the new chord.
		ForgetAll();
	}

	void CancelCapture()
	{
		g_capturing.store(false, std::memory_order_relaxed);
		g_captureSlot.store(-1, std::memory_order_relaxed);
		std::lock_guard lock(g_captureMutex);
		g_captureKeys.clear();
		g_captureDown.clear();
		g_captureResult.clear();
		g_captureResultSlot = -1;
		g_captureDone = false;
		g_captureOverflow = false;
	}

	bool CaptureActive()
	{
		return g_capturing.load(std::memory_order_relaxed);
	}

	std::string CaptureText()
	{
		std::lock_guard lock(g_captureMutex);
		if (g_captureKeys.empty()) {
			return {};
		}
		Chord chord;
		chord.triggers = g_captureKeys;
		return Format(chord);
	}

	bool CaptureOverflowed()
	{
		std::lock_guard lock(g_captureMutex);
		return g_captureOverflow;
	}

	bool TakeCaptureResult(Slot& a_slot, std::string& a_text)
	{
		std::lock_guard lock(g_captureMutex);
		if (!g_captureDone || g_captureResultSlot < 0) {
			return false;
		}
		a_slot = static_cast<Slot>(g_captureResultSlot);
		a_text = g_captureResult;
		g_captureDone = false;
		g_captureResult.clear();
		g_captureResultSlot = -1;
		g_captureKeys.clear();
		g_captureDown.clear();
		g_captureOverflow = false;
		g_capturing.store(false, std::memory_order_relaxed);
		g_captureSlot.store(-1, std::memory_order_relaxed);
		return true;
	}
}
