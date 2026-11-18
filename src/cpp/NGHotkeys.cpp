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
		struct Combo
		{
			bool valid{ false };
			bool ctrl{ false };
			bool shift{ false };
			bool alt{ false };
			std::uint32_t key{ 0 };
		};

		Combo      g_combo;
		Combo      g_quickCombo;
		std::mutex g_comboMutex;
		std::atomic<bool> g_latched{ false };
		std::atomic<bool> g_quickLatched{ false };
		bool       g_installed{ false };
		std::atomic<bool> g_textInput{ false };
		std::atomic<std::int64_t> g_textInputStamp{ 0 };

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

		Combo Parse(const std::string& a_text)
		{
			Combo result;

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
				if (lower == "ctrl" || lower == "control") {
					result.ctrl = true;
				} else if (lower == "shift") {
					result.shift = true;
				} else if (lower == "alt") {
					result.alt = true;
				} else if (const auto code = ScanCodeFor(lower); code != 0 && result.key == 0) {
					result.key = code;
				} else {
					SKSE::log::warn("Hotkeys: cannot parse '{}' in '{}'; random outfit hotkey disabled.", token, a_text);
					return Combo{};
				}
			}

			result.valid = result.key != 0;
			return result;
		}

		bool ModifiersHeld(const Combo& a_combo)
		{
			const bool ctrl = (::GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
			const bool shift = (::GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
			const bool alt = (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
			return ctrl == a_combo.ctrl && shift == a_combo.shift && alt == a_combo.alt;
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

				Combo combo;
				Combo quickCombo;
				{
					std::lock_guard lock(g_comboMutex);
					combo = g_combo;
					quickCombo = g_quickCombo;
				}
				if (!a_event || (!combo.valid && !quickCombo.valid)) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const bool inputBlocked = CachedTextInputActive();
				auto* ui = RE::UI::GetSingleton();
				const bool consoleOpen = ui && ui->IsMenuOpen(RE::Console::MENU_NAME);

				for (auto* event = *a_event; event; event = event->next) {
					auto* button = event->AsButtonEvent();
					if (!button || button->GetDevice() != RE::INPUT_DEVICE::kKeyboard) {
						continue;
					}
					const auto code = button->GetIDCode();

					// Random outfit hotkey.
					if (combo.valid && code == combo.key) {
						if (button->IsUp()) {
							g_latched.store(false, std::memory_order_relaxed);
						} else if (button->IsDown() && !g_latched.load(std::memory_order_relaxed) && !inputBlocked && !consoleOpen) {
							g_latched.store(true, std::memory_order_relaxed);
							if (ModifiersHeld(combo)) {
								Fire();
							}
						}
					}

					// Quick picker hotkey.
					if (quickCombo.valid && code == quickCombo.key) {
						if (button->IsUp()) {
							g_quickLatched.store(false, std::memory_order_relaxed);
						} else if (button->IsDown() && !g_quickLatched.load(std::memory_order_relaxed) && !inputBlocked && !consoleOpen) {
							g_quickLatched.store(true, std::memory_order_relaxed);
							if (ModifiersHeld(quickCombo)) {
								FireQuickPicker();
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
			SKSE::log::error("Hotkeys: the input device manager is not available; random outfit hotkey disabled.");
			return false;
		}

		Reload();
		manager->AddEventSink<RE::InputEvent*>(&g_sink);
		g_installed = true;
		SKSE::log::debug("Hotkeys: input sink registered (random '{}', quick picker '{}').", Settings::Get().randomOutfitHotkey, Settings::Get().quickPickerHotkey);
		return true;
	}

	void Reload()
	{
		const auto& settings = Settings::Get();
		const auto combo = Parse(settings.randomOutfitHotkey);
		const auto quickCombo = Parse(settings.quickPickerHotkey);
		{
			std::lock_guard lock(g_comboMutex);
			g_combo = combo;
			g_quickCombo = quickCombo;
		}
		g_latched.store(false, std::memory_order_relaxed);
		g_quickLatched.store(false, std::memory_order_relaxed);
		if (!combo.valid) {
			SKSE::log::debug("Hotkeys: random outfit hotkey is disabled.");
		}
		if (!quickCombo.valid) {
			SKSE::log::debug("Hotkeys: quick picker hotkey is disabled.");
		}
	}

	void SetTextInputActive(bool a_active)
	{
		const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
		g_textInputStamp.store(a_active ? now : 0, std::memory_order_relaxed);
		g_textInput.store(a_active, std::memory_order_relaxed);
	}
}
