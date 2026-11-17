#include "MenuUI.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "I18n.h"
#include "version.h"
#include "ImGuiApi.h"
#include "MenuBridge.h"
#include "MenuOutfitEditor.h"
#include "compat/ModCompat.h"
#include "compat/PadCompat.h"
#include "NGFeatures.h"
#include "OutfitClipboard.h"
#include "SMP.h"
#include "bindings.h"
#include "NGHotkeys.h"
#include "core/OutfitService.h"
#include "core/Settings.h"
#include "core/Types.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

namespace SOS::Menu
{
	namespace
	{
		constexpr const char* kSectionName = "Skyrim Outfit System";

		std::atomic<bool> g_registered{ false };
		std::mutex g_registerMutex;

		// ---- outfits browser state -----------------------------------------
		char                      g_search[128]{};
		bool                      g_searchFavoritesOnly{ false };
		int                       g_page{ 1 };
		std::vector<std::string>  g_filtered;
		std::string               g_filterSignature;
		std::string               g_selected;
		// Set after a paste (and anywhere else that picks an outfit
		// programmatically) so the list scrolls the selection into view on the
		// next frame instead of leaving it off screen.
		bool                      g_scrollToSelected{ false };
		char                      g_newName[257]{};
		char                      g_renameName[257]{};
		std::string               g_pendingDelete;
		// Result line of the last OutfitSystemData.json import / export.
		std::string               g_dataStatus;
		// Which actor the browser and the editor act on; 0 means the player.
		// The old MCM had the same "select the actor you are editing" control.
		RE::FormID                g_targetActor{ 0 };
		RE::FormID                g_pendingActorRemove{ 0 };

		// ---- quick picker (standalone window, opened by spell or hotkey) ----
		MenuBridge::WindowInterface* g_quickWindow{ nullptr };
		char                      g_quickSearch[128]{};
		bool                      g_quickFavoritesOnly{ false };
		std::vector<std::string>  g_quickFiltered;
		std::string               g_quickSignature;
		// The highlight is stored as an index into g_quickFiltered, not as a page
		// number, so it survives page size changes and reopening the window.
		int                       g_quickFocus{ 0 };
		bool                      g_quickScrollToFocus{ false };
		// The quick picker is reachable twice: as a standalone framework window
		// and as a page inside the F1 menu. Only the standalone case may open
		// its own ImGui window - inside the menu page one is already open.
		bool                      g_renderingSection{ false };
		// Set when the window opens so the saved position and size are applied
		// even though the ImGui context already knows the window from earlier.
		std::atomic<bool>         g_quickGeometryForce{ false };
		std::atomic<bool>         g_quickRefreshPending{ false };

		// Raw gamepad state, fed by the menu framework's input-event callback.
		// ImGui only receives gamepad keys when the framework's own main menu
		// is the active nav window; a standalone AddWindow does not get that
		// treatment on every framework build. The callback bypasses ImGui
		// entirely and only records edges/held state for the render thread.
		std::atomic<bool>         g_rawGamepad{ false };
		std::atomic<bool>         g_padUp{ false };
		std::atomic<bool>         g_padDown{ false };
		std::atomic<bool>         g_padPrev{ false };
		std::atomic<bool>         g_padNext{ false };
		std::atomic<bool>         g_padWear{ false };
		std::atomic<bool>         g_padRandom{ false };
		std::atomic<bool>         g_padRemove{ false };
		std::atomic<bool>         g_padClose{ false };

		RE::Actor* TargetActor()
		{
			if (g_targetActor == 0) {
				return RE::PlayerCharacter::GetSingleton();
			}
			return RE::TESForm::LookupByID<RE::Actor>(g_targetActor);
		}

		const char* TargetName()
		{
			auto* actor = TargetActor();
			if (!actor) {
				return I18n::T("<not loaded>", "<未加载>");
			}
			const char* name = actor->GetDisplayFullName();
			return name ? name : I18n::T("<not loaded>", "<未加载>");
		}

		std::string Lower(std::string a_text)
		{
			std::transform(a_text.begin(), a_text.end(), a_text.begin(), [](unsigned char c) {
				return static_cast<char>(std::tolower(c));
			});
			return a_text;
		}

		// Searches without allocating a lowered copy of every outfit name. The
		// needle is already lower case; the haystack is folded one character at
		// a time. With 1000+ outfits this saves a heap allocation per search.
		bool ContainsNoCase(std::string_view a_text, std::string_view a_needle)
		{
			if (a_needle.empty()) {
				return true;
			}
			return std::search(a_text.begin(), a_text.end(), a_needle.begin(), a_needle.end(),
				[](char a_lhs, char a_rhs) {
					return static_cast<char>(std::tolower(static_cast<unsigned char>(a_lhs))) == a_rhs;
				}) != a_text.end();
		}

		// ---- outfit name cache -------------------------------------------------
		// all_outfit_names() builds a sorted copy of the whole store. The actors
		// panel asks for it once per open combo, which used to be 13 copies per
		// frame with a 1,000 outfit list. Cache it until the store revision moves.
		std::vector<std::string> g_outfitNamesCache;
		std::uint64_t            g_outfitNamesCacheRevision{ ~std::uint64_t{ 0 } };

		const std::vector<std::string>& OutfitNamesCache()
		{
			// Callers may or may not hold a guard already; the mutex is recursive,
			// so taking one here keeps the cache safe from the Papyrus thread.
			auto guard = outfit_service_get_singleton_ptr();
			auto& service = guard->inner();
			const auto revision = service.revision();
			if (revision != g_outfitNamesCacheRevision) {
				const auto all = service.all_outfit_names();
				g_outfitNamesCache.clear();
				g_outfitNamesCache.reserve(all.size());
				for (const auto& name : all) {
					g_outfitNamesCache.emplace_back(name.c_str());
				}
				g_outfitNamesCacheRevision = revision;
			}
			return g_outfitNamesCache;
		}

		// ---- list layout helpers ---------------------------------------------
		//
		// Both page sizes live in the ini ([UI] OutfitPageSize / QuickPickerPageSize)
		// so the choice survives a restart instead of resetting every time the
		// window is opened.
		int OutfitPageSize()
		{
			return std::clamp(Settings::Get().outfitPageSize, 5, 500);
		}

		int QuickPageSize()
		{
			return std::clamp(Settings::Get().quickPickerPageSize, 5, 500);
		}

		int QuickPageCount()
		{
			const auto size = static_cast<std::size_t>(QuickPageSize());
			return std::max(1, static_cast<int>((g_quickFiltered.size() + size - 1) / size));
		}

		// 1-based page that contains the highlighted entry.
		int QuickPage()
		{
			const int size = QuickPageSize();
			if (g_quickFocus < 0) {
				g_quickFocus = 0;
			}
			return g_quickFocus / size + 1;
		}

		void QuickClampFocus()
		{
			if (g_quickFiltered.empty()) {
				g_quickFocus = 0;
				return;
			}
			g_quickFocus = std::clamp(g_quickFocus, 0, static_cast<int>(g_quickFiltered.size()) - 1);
		}

		double Seconds()
		{
			using clock = std::chrono::steady_clock;
			static const auto start = clock::now();
			return std::chrono::duration<double>(clock::now() - start).count();
		}

		// Dragging a slider fires every frame; writing the ini on every one of
		// those frames causes a visible hitch on large load orders. Coalesce the
		// writes to at most two per second and flush once the drag has settled.
		double g_lastSettingsSave = 0.0;
		bool   g_settingsSavePending = false;

		void SaveSettingsThrottled()
		{
			g_settingsSavePending = true;
			const double now = Seconds();
			if (now - g_lastSettingsSave >= 0.5) {
				Settings::Get().Save();
				g_lastSettingsSave = now;
				g_settingsSavePending = false;
			}
		}

		void FlushSettingsIfDue()
		{
			if (!g_settingsSavePending) {
				return;
			}
			const double now = Seconds();
			if (now - g_lastSettingsSave >= 0.5) {
				Settings::Get().Save();
				g_lastSettingsSave = now;
				g_settingsSavePending = false;
			}
		}

		// ---- quick picker geometry -------------------------------------------
		//
		// The menu framework host never writes an imgui.ini, so a window that is
		// not positioned explicitly opens centred again after every restart.
		// The four values therefore live in our ini: they are captured while the
		// window is drawn and flushed once the user stops dragging or resizing.
		bool   g_quickGeometryDirty{ false };
		double g_quickGeometryChangedAt{ 0.0 };

		void RememberQuickPickerGeometry(const ImVec2& a_pos, const ImVec2& a_size)
		{
			if (a_size.x < 64.0f || a_size.y < 64.0f) {
				return;
			}
			auto& settings = Settings::Get();
			const auto round = [](float a_value) {
				return static_cast<int>(a_value >= 0.0f ? a_value + 0.5f : a_value - 0.5f);
			};
			const int x = round(a_pos.x);
			const int y = round(a_pos.y);
			const int w = round(a_size.x);
			const int h = round(a_size.y);
			if (x == settings.quickPickerX && y == settings.quickPickerY &&
				w == settings.quickPickerWidth && h == settings.quickPickerHeight) {
				return;
			}
			settings.quickPickerX = x;
			settings.quickPickerY = y;
			settings.quickPickerWidth = w;
			settings.quickPickerHeight = h;
			g_quickGeometryDirty = true;
			g_quickGeometryChangedAt = Seconds();
		}

		// Saves the ini once the drag settled, so moving the window does not hit
		// the disk on every frame.
		void FlushQuickPickerGeometry(bool a_force = false)
		{
			if (!g_quickGeometryDirty) {
				return;
			}
			if (!a_force && Seconds() - g_quickGeometryChangedAt < 0.75) {
				return;
			}
			g_quickGeometryDirty = false;
			Settings::Get().Save();
		}

		// Holding a navigation key first repeats slowly, then speeds up, which is
		// what the gamepad d-pad needs to cross a long outfit list quickly.
		struct KeyRepeat
		{
			double next{ 0.0 };
			int    count{ 0 };
			bool   held{ false };
		};

		// A short first delay so a normal key press already repeats, then a fast
		// ramp: the player asked for "a slight hold should move".
		constexpr double kRepeatDelay{ 0.22 };
		constexpr double kRepeatMinInterval{ 0.030 };
		constexpr double kRepeatAcceleration{ 0.030 };

		bool TickRepeat(KeyRepeat& a_state, bool a_down)
		{
			if (!a_down) {
				a_state.held = false;
				a_state.count = 0;
				return false;
			}

			const double now = Seconds();
			if (!a_state.held) {
				a_state.held = true;
				a_state.count = 1;
				a_state.next = now + kRepeatDelay;
				return true;
			}
			if (now < a_state.next) {
				return false;
			}
			++a_state.count;
			const double interval = std::max(kRepeatMinInterval, kRepeatDelay - kRepeatAcceleration * (a_state.count - 1));
			a_state.next = now + interval;
			return true;
		}

		void MoveFocus(int a_delta)
		{
			if (g_quickFiltered.empty()) {
				return;
			}
			const int count = static_cast<int>(g_quickFiltered.size());
			g_quickFocus = std::clamp(g_quickFocus + a_delta, 0, count - 1);
			g_quickScrollToFocus = true;
		}

		void MovePage(int a_delta)
		{
			const int pageSize = QuickPageSize();
			const int count = static_cast<int>(g_quickFiltered.size());
			if (count == 0) {
				return;
			}
			const int row = g_quickFocus % pageSize;
			int       target = g_quickFocus + a_delta * pageSize;
			if (target < 0) {
				target = row;
			}
			if (target >= count) {
				target = count - 1;
			}
			g_quickFocus = std::clamp(target, 0, count - 1);
			g_quickScrollToFocus = true;
		}

		const char* StateName(StateType a_state)
		{
			switch (a_state) {
			case StateType::World: return "World";
			case StateType::Town: return "Town";
			case StateType::Dungeon: return "Dungeon";
			case StateType::WorldSnowy: return "World (snowy)";
			case StateType::TownSnowy: return "Town (snowy)";
			case StateType::DungeonSnowy: return "Dungeon (snowy)";
			case StateType::WorldRainy: return "World (rainy)";
			case StateType::TownRainy: return "Town (rainy)";
			case StateType::DungeonRainy: return "Dungeon (rainy)";
			case StateType::City: return "City";
			case StateType::CitySnowy: return "City (snowy)";
			case StateType::CityRainy: return "City (rainy)";
			case StateType::Combat: return "Combat";
			default: return "?";
			}
		}

		constexpr std::array<StateType, 13> kAllStates{
			StateType::World, StateType::Town, StateType::Dungeon,
			StateType::WorldSnowy, StateType::TownSnowy, StateType::DungeonSnowy,
			StateType::WorldRainy, StateType::TownRainy, StateType::DungeonRainy,
			StateType::City, StateType::CitySnowy, StateType::CityRainy,
			StateType::Combat
		};

		// Rebuilds the filtered name list when the search input or the store changed.
		void RefreshList()
		{
			auto service = outfit_service_get_singleton_ptr();
			auto& inner = service->inner();

			// Check the cheap revision/search signature before copying every
			// outfit name. The revision also catches renames and favourite
			// changes, which keep the list size unchanged.
			std::string signature = std::to_string(inner.revision()) + "|" + g_search + "|" + (g_searchFavoritesOnly ? "1" : "0");
			if (signature == g_filterSignature) {
				return;
			}
			g_filterSignature = std::move(signature);

			const auto& all = OutfitNamesCache();
			const auto needle = Lower(g_search);
			g_filtered.clear();
			for (const auto& name : all) {
				// Only pay for the hash lookup when the favourites filter is on.
				if (g_searchFavoritesOnly) {
					const auto* outfit = inner.get_outfit_ptr(name.c_str());
					if (!outfit || !outfit->favorite_c()) {
						continue;
					}
				}
				if (!needle.empty() && !ContainsNoCase(std::string_view(name.c_str()), needle)) {
					continue;
				}
				g_filtered.push_back(name);
			}

			const auto pageSize = static_cast<std::size_t>(OutfitPageSize());
			const int  pages = std::max(1, static_cast<int>((g_filtered.size() + pageSize - 1) / pageSize));
			g_page = std::clamp(g_page, 1, pages);
		}

		void ApplyTo(RE::Actor* a_actor, const std::string& a_outfit)
		{
			if (!a_actor) {
				return;
			}
			if (!Features::AssignOutfit(a_actor, a_outfit.c_str())) {
				RE::SendHUDMessage::ShowHUDMessage(I18n::T("Skyrim Outfit System: that outfit no longer exists.", "Skyrim Outfit System：该幻化已不存在。"), nullptr, false);
				return;
			}
			const auto message = std::string(I18n::T("Outfit: ", "幻化：")) + a_outfit;
			RE::SendHUDMessage::ShowHUDMessage(message.c_str(), nullptr, false);
		}

		void ClearPadState()
		{
			g_padUp.store(false, std::memory_order_relaxed);
			g_padDown.store(false, std::memory_order_relaxed);
			g_padPrev.store(false, std::memory_order_relaxed);
			g_padNext.store(false, std::memory_order_relaxed);
			g_padWear.store(false, std::memory_order_relaxed);
			g_padRandom.store(false, std::memory_order_relaxed);
			g_padRemove.store(false, std::memory_order_relaxed);
			g_padClose.store(false, std::memory_order_relaxed);
		}

		void CloseQuickPicker()
		{
			ClearPadState();
			if (g_quickWindow) {
				g_quickWindow->IsOpen = false;
			}
		}

		void QuickRemoveOutfit(RE::Actor* a_actor)
		{
			if (!a_actor) {
				return;
			}
			outfit_service_get_mut_singleton_ptr()->inner().set_outfit_c("", a_actor->GetFormID());
			Features::RefreshActor(a_actor);
			RE::SendHUDMessage::ShowHUDMessage(I18n::T("Outfit removed.", "已取消幻化。"), nullptr, false);
		}

		void QuickWearFocused(RE::Actor* a_actor)
		{
			if (!a_actor || g_quickFiltered.empty()) {
				return;
			}
			QuickClampFocus();
			ApplyTo(a_actor, g_quickFiltered[static_cast<std::size_t>(g_quickFocus)]);
			CloseQuickPicker();
		}

		void QuickRandom(RE::Actor* a_actor)
		{
			if (!a_actor) {
				return;
			}
			const auto chosen = Features::PickRandomOutfit(a_actor, g_quickFavoritesOnly);
			if (chosen.empty()) {
				return;
			}
			const auto message = std::string(I18n::T("Outfit: ", "幻化：")) + chosen;
			RE::SendHUDMessage::ShowHUDMessage(message.c_str(), nullptr, false);
			// The window stays open and follows the roll, so the list shows what
			// the character is wearing now.
			for (std::size_t i = 0; i < g_quickFiltered.size(); ++i) {
				if (g_quickFiltered[i] == chosen) {
					g_quickFocus = static_cast<int>(i);
					g_quickScrollToFocus = true;
					break;
				}
			}
		}

		// Filters the outfit list for the quick picker window.
		void RefreshQuickList()
		{
			auto service = outfit_service_get_singleton_ptr();
			auto& inner = service->inner();

			std::string signature = std::to_string(inner.revision()) + "|" + g_quickSearch + "|" + (g_quickFavoritesOnly ? "1" : "0");
			if (signature == g_quickSignature) {
				return;
			}
			g_quickSignature = std::move(signature);

			const auto& all = OutfitNamesCache();
			const auto needle = Lower(g_quickSearch);
			g_quickFiltered.clear();
			for (const auto& name : all) {
				if (g_quickFavoritesOnly) {
					const auto* outfit = inner.get_outfit_ptr(name.c_str());
					if (!outfit || !outfit->favorite_c()) {
						continue;
					}
				}
				if (!needle.empty() && !ContainsNoCase(std::string_view(name.c_str()), needle)) {
					continue;
				}
				g_quickFiltered.push_back(name);
			}

			QuickClampFocus();
		}

		// Puts the highlight on the outfit the player currently wears, so opening
		// the window always starts where the character actually is.
		void FocusCurrentOutfit()
		{
			g_quickFocus = 0;
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return;
			}
			std::string want;
			{
				auto service = outfit_service_get_singleton_ptr();
				if (const auto* worn = service->inner().current_outfit_ptr(player->GetFormID())) {
					want = worn->name_c();
				}
			}
			if (want.empty()) {
				return;
			}
			for (std::size_t i = 0; i < g_quickFiltered.size(); ++i) {
				if (g_quickFiltered[i] == want) {
					g_quickFocus = static_cast<int>(i);
					g_quickScrollToFocus = true;
					return;
				}
			}
		}

		// The popup behind the "Change Outfit" lesser power and the quick picker
		// hotkey. It has to draw its own window: the menu framework calls every
		// registered window render function without an ImGui::Begin/End pair.
		void RenderQuickPickerBody()
		{
			if (g_quickRefreshPending.exchange(false, std::memory_order_acq_rel)) {
				g_quickSignature.clear();
				RefreshQuickList();
				FocusCurrentOutfit();
				g_quickScrollToFocus = true;
			}
			auto& settings = Settings::Get();
			auto* player = RE::PlayerCharacter::GetSingleton();

			IG::Text("Change outfit");
			IG::SameLine();
			IG::TextDisabled("%zu outfits", outfit_service_get_singleton_ptr()->inner().outfit_count());

			IG::SetNextItemWidth(300.0f);
			if (IG::InputTextWithHint("##quicksearch", "Search...", g_quickSearch, sizeof(g_quickSearch))) {
				g_quickSignature.clear();
				g_quickFocus = 0;
			}
			IG::SameLine();
			if (IG::Checkbox("Favourites", &g_quickFavoritesOnly)) {
				g_quickSignature.clear();
				g_quickFocus = 0;
			}
			IG::SameLine();
			IG::SetNextItemWidth(140.0f);
			if (IG::SliderInt("Page size", &settings.quickPickerPageSize, 5, 500)) {
				settings.quickPickerPageSize = std::clamp(settings.quickPickerPageSize, 5, 500);
				SaveSettingsThrottled();
			}

			RefreshQuickList();
			QuickClampFocus();

			const int pageSize = QuickPageSize();
			const int pageCount = QuickPageCount();

			IG::SameLine();
			if (IG::SmallButton("|<")) {
				g_quickFocus = 0;
				g_quickScrollToFocus = true;
			}
			IG::SameLine();
			if (IG::SmallButton("<")) {
				MovePage(-1);
			}
			IG::SameLine();
			IG::Text("%d/%d", QuickPage(), pageCount);
			IG::SameLine();
			if (IG::SmallButton(">")) {
				MovePage(1);
			}
			IG::SameLine();
			if (IG::SmallButton(">|")) {
				g_quickFocus = g_quickFiltered.empty() ? 0 : static_cast<int>(g_quickFiltered.size()) - 1;
				g_quickScrollToFocus = true;
			}

			// ---- keyboard and gamepad navigation ------------------------------
			// The framework feeds the keyboard and the gamepad into ImGui as key
			// events while one of its windows is open, so the same checks work for
			// both devices. Typing in the search box must never move the highlight.
			const auto& api = ImGuiApi::Get();
			const bool  typing = api.IsAnyItemActive && api.IsAnyItemActive();
			const bool  rawGamepad = g_rawGamepad.load(std::memory_order_acquire);

			// The raw callback only records the state; repeat timing stays on the
			// render thread and shares the same KeyRepeat curve as the keyboard.
			const bool padUp = rawGamepad ? g_padUp.load(std::memory_order_relaxed) : IG::IsKeyDown(ImGuiKey_GamepadDpadUp);
			const bool padDown = rawGamepad ? g_padDown.load(std::memory_order_relaxed) : IG::IsKeyDown(ImGuiKey_GamepadDpadDown);
			const bool padPrev = rawGamepad ? g_padPrev.load(std::memory_order_relaxed) : IG::IsKeyDown(ImGuiKey_GamepadDpadLeft);
			const bool padNext = rawGamepad ? g_padNext.load(std::memory_order_relaxed) : IG::IsKeyDown(ImGuiKey_GamepadDpadRight);

			const bool upHeld = IG::IsKeyDown(ImGuiKey_W) || IG::IsKeyDown(ImGuiKey_UpArrow) || padUp;
			const bool downHeld = IG::IsKeyDown(ImGuiKey_S) || IG::IsKeyDown(ImGuiKey_DownArrow) || padDown;
			const bool prevHeld = IG::IsKeyDown(ImGuiKey_A) || IG::IsKeyDown(ImGuiKey_LeftArrow) || padPrev;
			const bool nextHeld = IG::IsKeyDown(ImGuiKey_D) || IG::IsKeyDown(ImGuiKey_RightArrow) || padNext;

			static KeyRepeat s_upRepeat;
			static KeyRepeat s_downRepeat;
			static KeyRepeat s_prevRepeat;
			static KeyRepeat s_nextRepeat;

			if (!typing) {
				if (TickRepeat(s_upRepeat, upHeld)) {
					MoveFocus(-1);
				}
				if (TickRepeat(s_downRepeat, downHeld)) {
					MoveFocus(1);
				}
				if (TickRepeat(s_prevRepeat, prevHeld)) {
					MovePage(-1);
				}
				if (TickRepeat(s_nextRepeat, nextHeld)) {
					MovePage(1);
				}

				if (IG::IsKeyPressed(ImGuiKey_E) || IG::IsKeyPressed(ImGuiKey_Enter) ||
					(!rawGamepad && IG::IsKeyPressed(ImGuiKey_GamepadFaceDown))) {
					QuickWearFocused(player);
				}
				if (IG::IsKeyPressed(ImGuiKey_R) || (!rawGamepad && IG::IsKeyPressed(ImGuiKey_GamepadFaceUp))) {
					QuickRandom(player);
				}
				if (IG::IsKeyPressed(ImGuiKey_X) || (!rawGamepad && IG::IsKeyPressed(ImGuiKey_GamepadFaceLeft))) {
					QuickRemoveOutfit(player);
				}
				if (rawGamepad) {
					if (g_padWear.exchange(false, std::memory_order_relaxed)) {
						QuickWearFocused(player);
					}
					if (g_padRandom.exchange(false, std::memory_order_relaxed)) {
						QuickRandom(player);
					}
					if (g_padRemove.exchange(false, std::memory_order_relaxed)) {
						QuickRemoveOutfit(player);
					}
				}
			} else if (rawGamepad) {
				// A button pressed while the search box was focused must not fire
				// later when the box loses focus.
				g_padWear.store(false, std::memory_order_relaxed);
				g_padRandom.store(false, std::memory_order_relaxed);
				g_padRemove.store(false, std::memory_order_relaxed);
			}
			// Esc always closes, also while the search box has focus.
			if (IG::IsKeyPressed(ImGuiKey_Escape) || (!rawGamepad && IG::IsKeyPressed(ImGuiKey_GamepadFaceRight)) ||
				(rawGamepad && g_padClose.exchange(false, std::memory_order_relaxed))) {
				CloseQuickPicker();
			}

			IG::Separator();

			std::string wornName;
			if (player) {
				auto service = outfit_service_get_singleton_ptr();
				if (const auto* worn = service->inner().current_outfit_ptr(player->GetFormID())) {
					wornName = worn->name_c();
				}
			}

			const int first = (QuickPage() - 1) * pageSize;
			const int last = std::min(static_cast<int>(g_quickFiltered.size()), first + pageSize);
			const float listHeight = std::max(180.0f, IG::ContentRegionAvail().y - 96.0f);

			if (IG::BeginChild("##quicklist", ImVec2(0.0f, listHeight), true, ImGuiWindowFlags_HorizontalScrollbar)) {
				for (int i = first; i < last; ++i) {
					const auto&       name = g_quickFiltered[static_cast<std::size_t>(i)];
					const bool        highlighted = (i == g_quickFocus);
					const bool        isWorn = (name == wornName);
					const std::string wornLabel = isWorn ? ("* " + name) : std::string();
					const char*       label = isWorn ? wornLabel.c_str() : name.c_str();

					IG::PushID(i);
					// Long names stay readable: the row is as wide as its text and the
					// child scrolls horizontally.
					const float rowWidth = IG::CalcTextSize(label).x + 24.0f;
					const bool  clicked = IG::Selectable(label, highlighted, 0, ImVec2(rowWidth, 0.0f));
					const bool  hovered = IG::IsItemHovered();
					if (clicked) {
						g_quickFocus = i;
					}
					// A double click keeps the old one-step behaviour for the mouse.
					if (hovered && IG::IsMouseDoubleClicked(0)) {
						g_quickFocus = i;
						QuickWearFocused(player);
					}
					if (highlighted && g_quickScrollToFocus) {
						IG::SetScrollHereY(0.5f);
						g_quickScrollToFocus = false;
					}
					IG::PopID();
				}
				if (g_quickFiltered.empty()) {
					IG::TextDisabled("No outfit matches the filter.");
				}
			}
			IG::EndChild();

			IG::Separator();
			if (IG::Button("Random")) {
				QuickRandom(player);
			}
			IG::SameLine();
			if (IG::Button("Remove outfit")) {
				QuickRemoveOutfit(player);
			}
			if (IG::IsItemHovered()) {
				IG::SetTooltip("Removes the outfit from the player and shows the real armour again (X).");
			}
			IG::SameLine();
			if (IG::Button("Close")) {
				CloseQuickPicker();
			}

			IG::TextDisabled("W/S: select   A/D: page   E: wear   R: random   X: remove   Esc: close");
		}

		void RenderQuickPicker()
		{
			// The framework calls a registered window render callback without a
			// Begin/End pair, so the standalone quick picker has to open its own
			// window. A page inside the F1 menu already runs in one. Detecting
			// that with ImGui::GetCurrentWindow() does not work: ImGui keeps an
			// implicit "Debug##Default" window current for the whole frame, so
			// every callback looked like "already inside a window" and the quick
			// picker was drawn into that hidden debug window - which is exactly
			// the "Debug" title users saw. A flag set by the section thunk is
			// unambiguous.
			const bool standalone = !g_renderingSection;
			bool       open = g_quickWindow ? g_quickWindow->IsOpen.load() : true;

			ImVec2 geometryPos{};
			ImVec2 geometrySize{};
			bool   haveGeometry = false;

			if (standalone) {
				if (!IG::WindowPlumbingReady()) {
					IG::TextDisabled("The menu framework does not expose the window functions; the quick picker cannot open.");
					return;
				}
				const auto& settings = Settings::Get();
				const float width = static_cast<float>(std::clamp(settings.quickPickerWidth, 320, 4096));
				const float height = static_cast<float>(std::clamp(settings.quickPickerHeight, 240, 4096));
				// FirstUseEver only works while ImGui has never seen the window. The
				// framework keeps one context for the whole session, so reopening
				// the picker has to force the stored rectangle instead.
				const ImGuiCond cond = g_quickGeometryForce.load(std::memory_order_acquire) ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
				g_quickGeometryForce.store(false, std::memory_order_relaxed);
				IG::SetNextWindowSize(ImVec2(width, height), cond);
				if (settings.quickPickerX >= 0 && settings.quickPickerY >= 0) {
					// Restore the place the user left the window at.
					IG::SetNextWindowPos(ImVec2(static_cast<float>(settings.quickPickerX), static_cast<float>(settings.quickPickerY)),
						cond, ImVec2(0.0f, 0.0f));
				} else {
					IG::SetNextWindowPos(IG::MainViewportCenter(), cond, ImVec2(0.5f, 0.5f));
				}
				// A stable ###id keeps the window state even when the visible title
				// is translated into Chinese.
				const std::string title = std::string(I18n::Tr("Skyrim Outfit System - Quick picker")) + "###SOSQuickPicker";
				// The picker has its own keyboard/gamepad handling below. It must
				// NOT use ImGuiWindowFlags_NoNavInputs: ImGui 1.90 makes that flag
				// clear io.NavActive for the whole context as soon as this window
				// becomes the nav window, which also killed the menu framework's
				// gamepad navigation. The widgets are made non-navigable with
				// ImGuiItemFlags_NoNav around the body instead.
				constexpr ImGuiWindowFlags kQuickFlags = ImGuiWindowFlags_NoCollapse;
				if (!IG::BeginWindow(title.c_str(), &open, kQuickFlags)) {
					// ImGui 1.90 still requires End() even when Begin() returned false
					// (Begin/End and BeginChild/EndChild are the documented exceptions).
					IG::EndWindow();
					const bool nowClosed = g_quickWindow && !open;
					// Losing the last resize because the window closed within the
					// throttle window is easy to reproduce; flush it on the way out.
					FlushQuickPickerGeometry(nowClosed);
					if (nowClosed) {
						g_quickWindow->IsOpen = false;
					}
					return;
				}
				haveGeometry = IG::GetWindowPos(&geometryPos) && IG::GetWindowSize(&geometrySize);
			}

			const bool itemFlagsPushed = IG::PushItemFlagNoNav();
			RenderQuickPickerBody();
			if (itemFlagsPushed) {
				IG::PopItemFlag();
			}

			if (standalone) {
				IG::EndWindow();
				if (haveGeometry) {
					RememberQuickPickerGeometry(geometryPos, geometrySize);
				}
				if (g_quickWindow && !open) {
					g_quickWindow->IsOpen = false;
					FlushQuickPickerGeometry(true);
				} else {
					FlushQuickPickerGeometry();
				}
			}
		}

		// ---- outfits panel --------------------------------------------------
		// Selects an outfit that was created or chosen in code (clipboard paste,
		// "create" buttons) and makes sure it is on the visible page; the row
		// then scrolls itself into view on the next frame.
		void FocusOutfitInList(const std::string& a_name)
		{
			g_selected = a_name;
			std::snprintf(g_renameName, sizeof(g_renameName), "%s", a_name.c_str());
			g_filterSignature.clear();
			RefreshList();
			const auto it = std::find(g_filtered.begin(), g_filtered.end(), a_name);
			if (it != g_filtered.end()) {
				const auto pageSize = static_cast<std::size_t>(std::max(1, OutfitPageSize()));
				g_page = static_cast<int>((static_cast<std::size_t>(it - g_filtered.begin()) / pageSize) + 1);
			}
			g_scrollToSelected = true;
		}

		void RenderOutfits()
		{
			auto& settings = Settings::Get();
			const std::size_t outfitCount = outfit_service_get_singleton_ptr()->inner().outfit_count();

			IG::Text("Outfits: %zu", outfitCount);
			IG::SameLine();
			IG::TextDisabled("(double-click a name to apply it to %s)", TargetName());

			IG::SetNextItemWidth(320.0f);
			if (IG::InputTextWithHint("##search", "Search by name...", g_search, sizeof(g_search))) {
				g_page = 1;
			}
			IG::SameLine();
			if (IG::Checkbox("Favourites only", &g_searchFavoritesOnly)) {
				g_page = 1;
			}

			RefreshList();

			const int  pageSize = OutfitPageSize();
			const int  pageCount = std::max(1, static_cast<int>((g_filtered.size() + static_cast<std::size_t>(pageSize) - 1) / static_cast<std::size_t>(pageSize)));
			// Deleting the last outfit of the last page (or a filter shrinking the
			// list) can leave g_page past the end; clamp before it is used to slice.
			g_page = std::clamp(g_page, 1, pageCount);
			IG::SameLine();
			if (IG::SmallButton("|<")) { g_page = 1; }
			IG::SameLine();
			if (IG::SmallButton("<")) { g_page = std::max(1, g_page - 1); }
			IG::SameLine();
			IG::Text("page %d / %d", g_page, pageCount);
			IG::SameLine();
			if (IG::SmallButton(">")) { g_page = std::min(pageCount, g_page + 1); }
			IG::SameLine();
			if (IG::SmallButton(">|")) { g_page = pageCount; }
			IG::SameLine();
			// The page size is stored in the ini, so it does not reset every time
			// the menu is opened.
			IG::SetNextItemWidth(140.0f);
			if (IG::SliderInt("Page size", &settings.outfitPageSize, 5, 500)) {
				settings.outfitPageSize = std::clamp(settings.outfitPageSize, 5, 500);
				g_page = 1;
				g_filterSignature.clear();
				SaveSettingsThrottled();
			}
			IG::SameLine();
			if (IG::BeginCombo("Apply to", TargetName())) {
				if (IG::Selectable(I18n::T("Player", "玩家"), g_targetActor == 0)) {
					g_targetActor = 0;
				}
				std::vector<RE::FormID> assignedActors;
				{
					auto service = outfit_service_get_singleton_ptr();
					for (const auto formId : service->inner().list_actors()) {
						assignedActors.push_back(formId);
					}
				}
				for (const auto formId : assignedActors) {
					auto* actor = RE::TESForm::LookupByID<RE::Actor>(formId);
					const char* name = I18n::T("<not loaded>", "<未加载>");
					if (actor) {
						if (const char* display = actor->GetDisplayFullName()) {
							name = display;
						}
					}
					IG::PushID(static_cast<int>(formId));
					if (IG::Selectable(name, g_targetActor == formId)) {
						g_targetActor = formId;
					}
					IG::PopID();
				}
				IG::EndCombo();
			}

			IG::Separator();

			if (!g_pendingDelete.empty()) {
				IG::TextWrapped("Delete outfit \"%s\"? This also removes it from every actor and state assignment.", g_pendingDelete.c_str());
				if (IG::Button("Delete")) {
					const bool removed = outfit_service_get_mut_singleton_ptr()->inner().delete_outfit(g_pendingDelete.c_str());
					SKSE::log::info("Outfits: deleted '{}': {}.", g_pendingDelete, removed ? "yes" : "no");
					if (g_selected == g_pendingDelete) {
						g_selected.clear();
					}
					g_filterSignature.clear();
					g_quickSignature.clear();
					g_filtered.clear();
					g_pendingDelete.clear();
					CreateRefreshAllArmorsTask();
				}
				IG::SameLine();
				if (IG::Button("Cancel")) {
					g_pendingDelete.clear();
				}
				IG::Separator();
			}

			const std::size_t begin = static_cast<std::size_t>(std::max(0, (g_page - 1) * pageSize));
			const std::size_t end = std::min(g_filtered.size(), begin + static_cast<std::size_t>(pageSize));

			auto* target = TargetActor();
			std::string wornName;
			if (target) {
				auto service = outfit_service_get_singleton_ptr();
				if (const auto* wornOutfit = service->inner().current_outfit_ptr(target->GetFormID())) {
					wornName = wornOutfit->name_c();
				}
			}

			// A horizontal scrollbar keeps long names readable instead of letting the
			// trailing buttons cut them off; the width is adjustable in the settings.
			const float listWidth = static_cast<float>(std::clamp(settings.outfitListWidth, 240, 1200));

			// Snapshot the visible page in one lock acquisition. Doing a lookup per
			// row would allocate and lock once per visible outfit on every frame.
			// Reuse the buffers across frames; the page can be up to 500 rows and
			// this loop runs every frame while the menu is open.
			static std::vector<char>        pageFavorites;
			static std::vector<std::size_t> pageArmorCounts;
			pageFavorites.assign(end - begin, 0);
			pageArmorCounts.assign(end - begin, 0);
			{
				auto service = outfit_service_get_singleton_ptr();
				for (std::size_t i = begin; i < end; ++i) {
					if (const auto* outfit = service->inner().get_outfit_ptr(g_filtered[i].c_str())) {
						pageFavorites[i - begin] = outfit->favorite_c() ? 1 : 0;
						pageArmorCounts[i - begin] = outfit->armors().size();
					}
				}
			}

			if (IG::BeginChild("##outfitlist", ImVec2(listWidth, 470.0f), true, ImGuiWindowFlags_HorizontalScrollbar)) {
				for (std::size_t i = begin; i < end; ++i) {
					const auto& name = g_filtered[i];
					const bool  isFavorite = pageFavorites[i - begin] != 0;
					const auto  armorCount = pageArmorCounts[i - begin];
					const bool  isSelected = (name == g_selected);
					// The old MCM put a marker next to the outfit the selected
					// actor was wearing; keep that so the list reads at a glance.
					const bool        isWorn = (name == wornName);
					const std::string wornLabel = isWorn ? ("* " + name) : std::string();
					const char*       label = isWorn ? wornLabel.c_str() : name.c_str();

					IG::PushID(static_cast<int>(i));

					// Keep the clickable name area and the trailing buttons apart:
					// a full-width Selectable would sit on top of them and swallow
					// every click (the old row made "Wear" and "Delete" unusable).
					constexpr float kButtonReserve = 150.0f;
					const float     textWidth = IG::CalcTextSize(label).x + 8.0f;
					const float     buttonX = std::max(textWidth + 8.0f, std::max(listWidth - kButtonReserve, 0.0f));
					const float     nameWidth = std::max(buttonX - 8.0f, textWidth);

					if (IG::Selectable(label, isSelected, 0, ImVec2(nameWidth, 0.0f))) {
						g_selected = name;
						std::snprintf(g_renameName, sizeof(g_renameName), "%s", name.c_str());
					}
					const bool rowHovered = IG::IsItemHovered();
					if (rowHovered && IG::IsMouseDoubleClicked(0)) {
						g_selected = name;
						ApplyTo(TargetActor(), name);
					}
					if (rowHovered) {
						IG::SetTooltip("%zu armour pieces\nWear applies it to the actor chosen in \"Apply to\".", armorCount);
					}
					if (isSelected && g_scrollToSelected) {
						IG::SetScrollHereY(0.5f);
						g_scrollToSelected = false;
					}

					IG::SameLine(buttonX);
					if (IG::SmallButton("Wear")) {
						ApplyTo(TargetActor(), name);
					}
					IG::SameLine();
					if (IG::SmallButton(isFavorite ? "*" : "-")) {
						outfit_service_get_mut_singleton_ptr()->inner().set_favorite(name.c_str(), !isFavorite);
						g_filterSignature.clear();
					}
					IG::SameLine();
					if (IG::SmallButton("Delete")) {
						g_pendingDelete = name;
					}

					IG::PopID();
				}
				if (g_filtered.empty()) {
					IG::TextDisabled("No outfits yet - wear some armour and use 'Create from worn' below.");
				}
			}
			IG::EndChild();

			IG::SameLine();
			if (IG::BeginChild("##outfiteditor", ImVec2(0.0f, 470.0f), true, ImGuiWindowFlags_HorizontalScrollbar)) {
				if (g_selected.empty()) {
					IG::TextDisabled("Select an outfit on the left to edit its armour pieces and slot policies.");
				} else {
					Editor::Render(g_selected, g_targetActor, [] { g_selected.clear(); });
				}
			}
			IG::EndChild();

			IG::Separator();
			// The old MCM refused names longer than the native limit instead of
			// letting the store silently truncate them.
			const auto maxNameLength = static_cast<std::size_t>(std::max<std::int32_t>(1, outfit_service_get_singleton_ptr()->inner().max_outfit_name_len()));

			IG::SetNextItemWidth(240.0f);
			IG::InputTextWithHint("##newname", "New outfit name...", g_newName, sizeof(g_newName));
			// Compute this after InputText: the widget edits the buffer in place,
			// so checking before it would validate the previous frame's text.
			const bool nameTooLong = std::strlen(g_newName) > maxNameLength;
			IG::SameLine();
			if (IG::Button("Create from worn")) {
				if (nameTooLong) {
					RE::SendHUDMessage::ShowHUDMessage(I18n::T("That outfit name is too long.", "幻化名称过长。"), nullptr, false);
				} else if (Features::CreateOutfitFromWorn(RE::PlayerCharacter::GetSingleton(), g_newName)) {
					g_selected = g_newName;
					g_newName[0] = 0;
					g_filterSignature.clear();
				}
			}
			IG::SameLine();
			if (IG::Button("Create empty")) {
				if (nameTooLong) {
					RE::SendHUDMessage::ShowHUDMessage(I18n::T("That outfit name is too long.", "幻化名称过长。"), nullptr, false);
				} else if (g_newName[0] != 0 && outfit_service_get_mut_singleton_ptr()->inner().add_outfit(g_newName)) {
					g_selected = g_newName;
					g_newName[0] = 0;
					g_filterSignature.clear();
				}
			}
			IG::SameLine();
			if (IG::Button("Copy")) {
				if (g_selected.empty()) {
					RE::SendHUDMessage::ShowHUDMessage(I18n::T("Select an outfit on the left first.", "请先在左侧选中一套幻化。"), nullptr, false);
				} else if (OutfitClipboard::Copy(g_selected)) {
					RE::SendHUDMessage::ShowHUDMessage(I18n::T("Outfit copied to the clipboard.", "幻化已复制到剪贴板。"), nullptr, false);
				} else {
					RE::SendHUDMessage::ShowHUDMessage(I18n::T("Could not copy the outfit to the clipboard.", "无法把幻化复制到剪贴板。"), nullptr, false);
				}
			}
			IG::SameLine();
			if (IG::Button("Paste")) {
				// The clipboard object is the same JSON shape the legacy
				// OutfitSystemData.json uses, so a preset copied from there (or
				// from another player) pastes straight into a new outfit.
				std::string created;
				switch (OutfitClipboard::Paste(created)) {
				case OutfitClipboard::Result::Ok:
					{
						FocusOutfitInList(created);
						g_quickSignature.clear();
						std::array<char, 320> message{};
						std::snprintf(message.data(), message.size(), I18n::T("Pasted outfit '%s'.", "已粘贴幻化「%s」。"), created.c_str());
						RE::SendHUDMessage::ShowHUDMessage(message.data(), nullptr, false);
					}
					break;
				case OutfitClipboard::Result::ClipboardEmpty:
					RE::SendHUDMessage::ShowHUDMessage(I18n::T("The clipboard does not contain any text.", "剪贴板里没有文本。"), nullptr, false);
					break;
				case OutfitClipboard::Result::ClipboardOpenFail:
					RE::SendHUDMessage::ShowHUDMessage(I18n::T("Another program is holding the clipboard; try again.", "其他程序正在占用剪贴板，请重试。"), nullptr, false);
					break;
				case OutfitClipboard::Result::InvalidJson:
					RE::SendHUDMessage::ShowHUDMessage(I18n::T("The clipboard does not contain valid JSON.", "剪贴板内容不是有效的 JSON。"), nullptr, false);
					break;
				case OutfitClipboard::Result::NoOutfit:
					RE::SendHUDMessage::ShowHUDMessage(I18n::T("The clipboard does not contain an outfit.", "剪贴板里不是幻化数据。"), nullptr, false);
					break;
				case OutfitClipboard::Result::NameTooLong:
				case OutfitClipboard::Result::CreateFailed:
				default:
					RE::SendHUDMessage::ShowHUDMessage(I18n::T("Could not create the pasted outfit.", "无法创建粘贴的幻化。"), nullptr, false);
					break;
				}
			}
			IG::TextDisabled("Max %zu bytes per name.", maxNameLength);

			if (!g_selected.empty()) {
				IG::SetNextItemWidth(240.0f);
				IG::InputText("##rename", g_renameName, sizeof(g_renameName));
				IG::SameLine();
				if (IG::Button("Rename")) {
					if (std::strlen(g_renameName) > maxNameLength) {
						RE::SendHUDMessage::ShowHUDMessage(I18n::T("That outfit name is too long.", "幻化名称过长。"), nullptr, false);
					} else {
						const auto result = outfit_service_get_mut_singleton_ptr()->inner().rename_outfit(g_selected.c_str(), g_renameName);
						if (result == 0) {
							g_selected = g_renameName;
							g_filterSignature.clear();
						} else {
							RE::SendHUDMessage::ShowHUDMessage(result == 2 ? I18n::T("That name is already taken.", "该名称已被占用。") : I18n::T("Outfit not found.", "找不到该幻化。"), nullptr, false);
						}
					}
				}
			}

			IG::Separator();
			IG::TextDisabled("Plugin enabled: %s | Random hotkey: %s", outfit_service_get_singleton_ptr()->inner().enabled_c() ? I18n::T("yes", "是") : I18n::T("no", "否"), settings.randomOutfitHotkey.c_str());
		}

		// ---- actors panel ---------------------------------------------------
		void RenderOneActor(RE::FormID a_formId)
		{
			auto* actor = RE::TESForm::LookupByID<RE::Actor>(a_formId);
			const char* name = I18n::T("<not loaded>", "<未加载>");
			if (actor) {
				if (const char* display = actor->GetDisplayFullName()) {
					name = display;
				}
			}

			IG::PushID(static_cast<int>(a_formId));

			if (IG::TreeNode(name)) {
				// Read what the widgets need, then release the store lock before
				// drawing. Holding it for the whole tree blocked the render thread
				// while an expanded actor entry was on screen.
				std::string currentName;
				bool        autoSwitch = false;
				{
					auto service = outfit_service_get_singleton_ptr();
					const auto current = service->inner().current_outfit_ptr(a_formId);
					currentName = current ? std::string(current->name_c()) : std::string();
					autoSwitch = service->inner().get_state_based_switching_enabled_c(a_formId);
				}

				const auto& names = OutfitNamesCache();

				if (IG::BeginCombo("Outfit", currentName.empty() ? "<none>" : currentName.c_str())) {
					if (IG::Selectable("<none>", currentName.empty())) {
						{
							auto service = outfit_service_get_mut_singleton_ptr();
							service->inner().set_outfit_c("", a_formId);
						}
						Features::RefreshActor(actor);
					}
					for (const auto& text : names) {
						const bool selected = (text == currentName);
						if (IG::Selectable(text.c_str(), selected)) {
							{
								auto service = outfit_service_get_mut_singleton_ptr();
								service->inner().set_outfit_c(text.c_str(), a_formId);
							}
							Features::RefreshActor(actor);
						}
					}
					IG::EndCombo();
				}

				if (IG::Checkbox("Location/state based switching", &autoSwitch)) {
					{
						auto service = outfit_service_get_mut_singleton_ptr();
						service->inner().set_state_based_switching_enabled(autoSwitch, a_formId);
					}
					if (autoSwitch) {
						Features::ApplyCurrentStateOutfit(actor);
					} else {
						Features::RefreshActor(actor);
					}
				}

				if (IG::TreeNode("State outfits")) {
					// Copy the 13 names once instead of asking the store inside each
					// combo, and reuse the cached outfit list for every state combo.
					std::array<std::string, kStateTypeCount> stateNames;
					{
						auto service = outfit_service_get_singleton_ptr();
						for (std::uint32_t i = 0; i < kStateTypeCount; ++i) {
							stateNames[i] = std::string(service->inner().get_state_outfit_name_c(static_cast<StateType>(i), a_formId));
						}
					}

					for (const auto state : kAllStates) {
						IG::PushID(static_cast<int>(state));
						const auto& stateName = stateNames[static_cast<std::size_t>(state)];
						const bool  isRandom = (stateName == OutfitService::kRandomStateOutfit);
						const char* preview = stateName.empty() ? "<inherit>" : (isRandom ? I18n::T("<random>", "<随机>") : stateName.c_str());
						if (IG::BeginCombo(StateName(state), preview)) {
							if (IG::Selectable("<inherit>", stateName.empty())) {
								{
									auto service = outfit_service_get_mut_singleton_ptr();
									service->inner().unset_state_outfit(state, a_formId);
								}
								Features::ApplyCurrentStateOutfit(actor);
							}
							if (IG::Selectable(I18n::T("<random>", "<随机>"), isRandom)) {
								{
									auto service = outfit_service_get_mut_singleton_ptr();
									service->inner().set_state_outfit(state, a_formId, OutfitService::kRandomStateOutfit);
								}
								Features::ApplyCurrentStateOutfit(actor);
							}
							for (const auto& text : names) {
								if (IG::Selectable(text.c_str(), text == stateName)) {
									{
										auto service = outfit_service_get_mut_singleton_ptr();
										service->inner().set_state_outfit(state, a_formId, text.c_str());
									}
									Features::ApplyCurrentStateOutfit(actor);
								}
							}
							IG::EndCombo();
						}
						IG::PopID();
					}
					IG::TreePop();
				}

				IG::SameLine();
				if (IG::SmallButton("Refresh")) {
					Features::RefreshActor(actor);
				}
				IG::SameLine();
				if (IG::SmallButton("Remove")) {
					g_pendingActorRemove = a_formId;
				}

				IG::TreePop();
			}

			IG::PopID();
		}

		void RenderActors()
		{
			if (auto* player = RE::PlayerCharacter::GetSingleton(); player && IG::Button("Add player")) {
				outfit_service_get_mut_singleton_ptr()->inner().add_actor(player->GetFormID());
			}
			IG::SameLine();
			if (IG::BeginCombo("##addnearby", "Add a nearby actor...")) {
				const auto nearby = Features::ActorsNearPlayer();
				if (nearby.empty()) {
					IG::TextDisabled("No other actor in this cell.");
				}
				for (auto* actor : nearby) {
					if (!actor) {
						continue;
					}
					const std::string name(actor->GetDisplayFullName() ? actor->GetDisplayFullName() : "<not loaded>");
					IG::PushID(static_cast<int>(actor->GetFormID()));
					if (IG::Selectable(name.c_str())) {
						outfit_service_get_mut_singleton_ptr()->inner().add_actor(actor->GetFormID());
					}
					IG::PopID();
				}
				IG::EndCombo();
			}
			IG::SameLine();
			if (IG::Button("Refresh all appearances")) {
				CreateRefreshAllArmorsTask();
			}
			IG::Separator();

			if (g_pendingActorRemove != 0) {
				if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(g_pendingActorRemove)) {
					const char* name = actor->GetDisplayFullName();
					IG::TextWrapped("Stop managing %s? The player cannot be removed.", name ? name : I18n::T("<not loaded>", "<未加载>"));
				} else {
					IG::TextWrapped("Stop managing that actor?");
				}
				if (IG::Button("Remove")) {
					auto* removedActor = RE::TESForm::LookupByID<RE::Actor>(g_pendingActorRemove);
					outfit_service_get_mut_singleton_ptr()->inner().remove_actor(g_pendingActorRemove);
					if (g_targetActor == g_pendingActorRemove) {
						g_targetActor = 0;
					}
					g_pendingActorRemove = 0;
					if (removedActor) {
						Features::RefreshActor(removedActor);
					}
				}
				IG::SameLine();
				if (IG::Button("Cancel")) {
					g_pendingActorRemove = 0;
				}
			}

			std::vector<RE::FormID> actors;
			{
				auto service = outfit_service_get_singleton_ptr();
				for (const auto id : service->inner().list_actors()) {
					actors.push_back(static_cast<RE::FormID>(id));
				}
			}

			if (actors.empty()) {
				IG::TextDisabled("No actors configured yet.");
			}
			for (const auto id : actors) {
				RenderOneActor(id);
			}
		}

		// ---- settings panel -------------------------------------------------
		void RenderSettings()
		{
			FlushSettingsIfDue();
			auto& settings = Settings::Get();

			IG::SeparatorText("General");
			// The runtime state lives in OutfitService (it is persisted in the co-save),
			// so derive the checkbox from it instead of the stale Settings mirror.
			bool enabled = outfit_service_get_singleton_ptr()->inner().enabled_c();
			if (IG::Checkbox("Enable outfit overrides", &enabled)) {
				settings.enabled = enabled;
				outfit_service_get_mut_singleton_ptr()->inner().set_enabled(enabled);
				CreateRefreshAllArmorsTask();
				Settings::Get().Save();
			}
			bool quickslots = settings.quickslotsEnabled;
			if (IG::Checkbox("Give the player the \"Change Outfit\" power", &quickslots)) {
				settings.quickslotsEnabled = quickslots;
				Features::ApplyQuickslotSpell();
				Settings::Get().Save();
			}
			IG::TextDisabled("Version %s", SKYRIMOUTFITSYSTEMSE_VERSION_VERSTRING);
			IG::TextDisabled("Settings are stored in Data\\SKSE\\Plugins\\SkyrimOutfitSystemSE.ini");

			IG::SeparatorText("Outfit data");
			IG::TextDisabled("Data\\SKSE\\Plugins\\OutfitSystemData.json - the same file the original mod reads and writes.");
			if (IG::Button("Export outfits to OutfitSystemData.json")) {
				g_dataStatus.clear();
				std::string message;
				const bool ok = Features::ExportOutfitsToJson(message);
				g_dataStatus = message;
				RE::SendHUDMessage::ShowHUDMessage(message.c_str(), nullptr, false);
				SKSE::log::info("Menu: export to OutfitSystemData.json {}.", ok ? "succeeded" : "failed");
			}
			IG::SameLine();
			if (IG::Button("Import outfits from OutfitSystemData.json")) {
				g_dataStatus.clear();
				std::string message;
				const bool ok = Features::ImportOutfitsFromJson(message);
				g_dataStatus = message;
				RE::SendHUDMessage::ShowHUDMessage(message.c_str(), nullptr, false);
				SKSE::log::info("Menu: import from OutfitSystemData.json {}.", ok ? "succeeded" : "failed");
				if (ok) {
					g_filterSignature.clear();
					g_selected.clear();
					CreateRefreshAllArmorsTask();
				}
			}
			if (!g_dataStatus.empty()) {
				IG::TextWrapped("%s", g_dataStatus.c_str());
			}

			IG::SeparatorText("Language");
			{
				// Every file in Data/interface/translations becomes a choice, so a
				// translator can add a language without touching the plugin.
				const std::string mode(I18n::ModeName());
				const bool        isAuto = mode == "auto";
				const char*       preview = isAuto ? I18n::Tr("Auto (game language)") : I18n::LabelForCode(I18n::CurrentCode());
				if (IG::BeginCombo("Language", preview)) {
					if (IG::Selectable("Auto (game language)", isAuto)) {
						I18n::SetLanguage("auto");
						Settings::Get().Save();
					}
					for (const auto& code : I18n::Languages()) {
						const bool selected = !isAuto && mode == code;
						if (IG::Selectable(I18n::LabelForCode(code.c_str()), selected)) {
							I18n::SetLanguage(code.c_str());
							Settings::Get().Save();
						}
					}
					IG::EndCombo();
				}
				if (!I18n::ChineseAvailable()) {
					IG::TextDisabled("Chinese needs [Fonts] EnableChinese = true and a CJK font (CN.ttf) in Data\\SKSE\\Plugins\\Fonts of the SKSE Menu Framework.");
				}
				if (IG::Button("Reload the language files")) {
					I18n::ReloadFile();
					RE::SendHUDMessage::ShowHUDMessage(I18n::T("Translation file reloaded.", "语言文件已重新加载。"), nullptr, false);
				}
				IG::SameLine();
				IG::TextDisabled("Translation file: %s", I18n::FileStatus());
				IG::TextDisabled("Data\\interface\\translations\\SkyrimOutfitSystemNG_<language>.txt - one file per language.");
				IG::TextDisabled("Menu titles update after a game restart (the framework caches them).");
			}

			IG::SeparatorText("Mod compatibility");
			const auto& compat = Compat::GetStatus();
			bool andEnabled = settings.compatAdvancedNudityDetection;
			if (IG::Checkbox("Advanced Nudity Detection sees the outfit", &andEnabled)) {
				settings.compatAdvancedNudityDetection = andEnabled;
				Settings::Get().Save();
			}
			if (!compat.andDetected) {
				IG::TextDisabled("Advanced Nudity Detection: not installed.");
			} else if (!compat.andHooked) {
				IG::TextDisabled("Advanced Nudity Detection %s: its nudity factions were not found, detection keeps the vanilla behaviour.",
					compat.andVersion.empty() ? "" : compat.andVersion.c_str());
			} else {
				IG::TextDisabled("Advanced Nudity Detection %s: nudity, topless and bottomless follow the outfit.",
					compat.andVersion.empty() ? "" : compat.andVersion.c_str());
			}
			IG::TextDisabled("An empty outfit counts as bare skin.");
			bool padEnabled = settings.compatProceduralArmorDamage;
			if (IG::Checkbox("Procedural Armor Damage tears the outfit", &padEnabled)) {
				settings.compatProceduralArmorDamage = padEnabled;
				Settings::Get().Save();
			}
			if (!compat.padDetected) {
				IG::TextDisabled("Procedural Armor Damage: not installed.");
			} else {
				IG::TextDisabled("Procedural Armor Damage %s: damage is stored on the real armour worn in the same slot.",
					compat.padVersion.empty() ? "" : compat.padVersion.c_str());
			}
			IG::TextDisabled("Pieces shown over a slot with no real armour underneath cannot be damaged.");
			bool smpReset = settings.autoResetSmp;
			if (IG::Checkbox("Reset SMP physics when the outfit changes", &smpReset)) {
				settings.autoResetSmp = smpReset;
				Settings::Get().Save();
			}
			IG::TextDisabled("Only the actor whose outfit changed is reset.");
			if (!SMP::IsAvailable()) {
				IG::TextDisabled("Faster HDT-SMP (DynamicHDT): not detected; this switch has no effect.");
			} else {
				IG::TextDisabled("Faster HDT-SMP (DynamicHDT): detected.");
			}
			bool sceneOutfits = settings.sceneOutfitSwitching;
			if (IG::Checkbox("Wear <current outfit>_sex during SexLab / OStim scenes", &sceneOutfits)) {
				settings.sceneOutfitSwitching = sceneOutfits;
				Settings::Get().Save();
			}
			IG::TextDisabled("If the current outfit already ends with _sex it is kept unchanged.");
			IG::TextDisabled("The previous outfit is restored when the scene ends.");
			if (IG::Button("Log slot report")) {
				Compat::Pad::Diagnose(RE::PlayerCharacter::GetSingleton());
				RE::SendHUDMessage::ShowHUDMessage(I18n::T("Slot report written to SkyrimOutfitSystemNG.log", "槽位报告已写入 SkyrimOutfitSystemNG.log"), nullptr, false);
			}
			IG::SameLine();
			IG::TextDisabled("Lists every slot: what the outfit shows, what is really worn.");

			IG::SeparatorText("Hotkeys");
			static char        hotkeyBuffer[64]{};
			static char        quickHotkeyBuffer[64]{};
			static std::string hotkeyShown;
			static std::string quickHotkeyShown;
			// Refresh the editable buffers when the ini value changes externally,
			// but not while the user is typing into them.
			if (hotkeyShown != settings.randomOutfitHotkey) {
				std::snprintf(hotkeyBuffer, sizeof(hotkeyBuffer), "%s", settings.randomOutfitHotkey.c_str());
				hotkeyShown = settings.randomOutfitHotkey;
			}
			if (quickHotkeyShown != settings.quickPickerHotkey) {
				std::snprintf(quickHotkeyBuffer, sizeof(quickHotkeyBuffer), "%s", settings.quickPickerHotkey.c_str());
				quickHotkeyShown = settings.quickPickerHotkey;
			}
			IG::SetNextItemWidth(200.0f);
			if (IG::InputTextWithHint("##hotkey", "ctrl+shift+O", hotkeyBuffer, sizeof(hotkeyBuffer))) {
				settings.randomOutfitHotkey = hotkeyBuffer;
				hotkeyShown = settings.randomOutfitHotkey;
				Hotkeys::Reload();
				SaveSettingsThrottled();
			}
			IG::SameLine();
			IG::TextDisabled("random outfit - example: ctrl+shift+O, alt+F5");
			IG::SetNextItemWidth(200.0f);
			if (IG::InputTextWithHint("##quickhotkey", "ctrl+shift+L", quickHotkeyBuffer, sizeof(quickHotkeyBuffer))) {
				settings.quickPickerHotkey = quickHotkeyBuffer;
				quickHotkeyShown = settings.quickPickerHotkey;
				Hotkeys::Reload();
				SaveSettingsThrottled();
			}
			IG::SameLine();
			IG::TextDisabled("quick picker window");

			IG::SeparatorText("Random outfit");
			if (IG::Checkbox("Favourites only", &settings.randomFavoritesOnly)) {
				Settings::Get().Save();
			}
			if (IG::Checkbox("Never pick the outfit that is already worn", &settings.randomExcludeCurrent)) {
				Settings::Get().Save();
			}
			bool quickslotRandom = settings.quickslotRandom;
			if (IG::Checkbox("The \"Change Outfit\" power rolls a random outfit", &quickslotRandom)) {
				settings.quickslotRandom = quickslotRandom;
				Settings::Get().Save();
			}
			IG::TextDisabled("Off (default) opens the quick picker window instead.");
			if (IG::Button("Roll now")) {
				const auto chosen = Features::PickRandomOutfit(RE::PlayerCharacter::GetSingleton(), settings.randomFavoritesOnly);
				const auto message = chosen.empty() ? std::string(I18n::T("No outfits available.", "没有可用幻化。")) : (std::string(I18n::T("Outfit: ", "幻化：")) + chosen);
				RE::SendHUDMessage::ShowHUDMessage(message.c_str(), nullptr, false);
			}

			IG::SeparatorText("Interface");
			{
				bool layoutChanged = false;
				if (IG::SliderInt("List width", &settings.outfitListWidth, 240, 1200)) {
					settings.outfitListWidth = std::clamp(settings.outfitListWidth, 240, 1200);
					layoutChanged = true;
				}
				if (IG::SliderInt("Outfit page size", &settings.outfitPageSize, 5, 500)) {
					settings.outfitPageSize = std::clamp(settings.outfitPageSize, 5, 500);
					g_filterSignature.clear();
					layoutChanged = true;
				}
				if (IG::SliderInt("Quick picker page size", &settings.quickPickerPageSize, 5, 500)) {
					settings.quickPickerPageSize = std::clamp(settings.quickPickerPageSize, 5, 500);
					layoutChanged = true;
				}
				if (layoutChanged) {
					SaveSettingsThrottled();
				}
				IG::TextDisabled("List width and page sizes are stored in the ini and shared by the quick picker.");
			}

			IG::SeparatorText("Diagnostics");
			bool extraLogging = settings.extraLogging;
			if (IG::Checkbox("Verbose logging (takes effect on the next start)", &extraLogging)) {
				settings.extraLogging = extraLogging;
				Settings::Get().Save();
			}
			IG::TextDisabled("ImGui host: %s", ImGuiApi::HostName());
		}

		void UpdateTextInputState()
		{
			// ImGui is not thread-safe: sample the focused-widget state here on the
			// render thread and let the input sink read the cached value.
			Hotkeys::SetTextInputActive(MenuBridge::TextInputActive());
		}

		bool __stdcall OnFrameworkInput(RE::InputEvent* a_event)
		{
			if (!g_quickWindow || !g_quickWindow->IsOpen.load(std::memory_order_acquire)) {
				ClearPadState();
				return false;
			}

			bool handled = false;
			for (auto* event = a_event; event; event = event->next) {
				auto* button = event->AsButtonEvent();
				if (!button || button->GetDevice() != RE::INPUT_DEVICE::kGamepad) {
					continue;
				}

				switch (button->GetIDCode()) {
				case static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Key::kUp):
					g_padUp.store(button->IsPressed(), std::memory_order_relaxed);
					handled = true;
					break;
				case static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Key::kDown):
					g_padDown.store(button->IsPressed(), std::memory_order_relaxed);
					handled = true;
					break;
				case static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Key::kLeft):
					g_padPrev.store(button->IsPressed(), std::memory_order_relaxed);
					handled = true;
					break;
				case static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Key::kRight):
					g_padNext.store(button->IsPressed(), std::memory_order_relaxed);
					handled = true;
					break;
				case static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Key::kA):
					if (button->IsDown()) {
						g_padWear.store(true, std::memory_order_relaxed);
					}
					handled = true;
					break;
				case static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Key::kY):
					if (button->IsDown()) {
						g_padRandom.store(true, std::memory_order_relaxed);
					}
					handled = true;
					break;
				case static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Key::kX):
					if (button->IsDown()) {
						g_padRemove.store(true, std::memory_order_relaxed);
					}
					handled = true;
					break;
				case static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Key::kB):
					if (button->IsDown()) {
						g_padClose.store(true, std::memory_order_relaxed);
					}
					handled = true;
					break;
				default:
					break;
				}
			}
			return handled;
		}

		void RenderQuickPickerThunk() { UpdateTextInputState(); FlushSettingsIfDue(); RenderQuickPicker(); }
// Same body, but flagged as "already inside the menu window".
void RenderQuickPickerSectionThunk()
{
	UpdateTextInputState();
	g_renderingSection = true;
	RenderQuickPicker();
	g_renderingSection = false;
}
		void RenderOutfitsThunk() { UpdateTextInputState(); FlushSettingsIfDue(); RenderOutfits(); }
		void RenderActorsThunk() { UpdateTextInputState(); FlushSettingsIfDue(); RenderActors(); }
		void RenderSettingsThunk() { UpdateTextInputState(); FlushSettingsIfDue(); RenderSettings(); }
	}

	bool Register()
	{
		if (g_registered.load(std::memory_order_acquire)) {
			return true;
		}

		std::lock_guard lock(g_registerMutex);
		if (g_registered.load(std::memory_order_acquire)) {
			return true;
		}

		if (!MenuBridge::EnsureLoaded()) {
			LOG(warn, "Menu: the SKSE Menu Framework is not installed; the settings menu stays unavailable.");
			return false;
		}

		if (!ImGuiApi::Bind()) {
			LOG(err, "Menu: {}", ImGuiApi::Status());
			return false;
		}

		// A standalone window so the quick slot power can show it on demand; the
		// section item is the fallback for users who prefer the F1 menu.
		g_quickWindow = MenuBridge::AddWindow(RenderQuickPickerThunk);
		if (g_quickWindow) {
			LOG(debug, "Menu: quick picker window created.");
			// 3.18 is the oldest framework build we have confirmed working with the
			// quick picker; 3.9 and older never deliver gamepad keys to a custom
			// AddWindow window. Keep a raw-input fallback only below 3.18. On 3.18+
			// stay on the framework's own translation path: registering the callback
			// would remove every gamepad event from the queue before the framework's
			// menu sees it.
			const float frameworkVersion = MenuBridge::FrameworkVersion();
			const bool  legacyGamepad = frameworkVersion <= 0.0f || frameworkVersion < 3.18f;
			if (legacyGamepad && MenuBridge::RegisterInputEvent(OnFrameworkInput)) {
				g_rawGamepad.store(true, std::memory_order_release);
				LOG(info, "Menu: raw gamepad input enabled for the quick picker (framework {} is older than 3.18).", frameworkVersion);
			} else {
				g_rawGamepad.store(false, std::memory_order_release);
				if (!legacyGamepad) {
					LOG(info, "Menu: SKSE Menu Framework {} handles gamepad input for the quick picker.", frameworkVersion);
				}
			}
		} else {
			LOG(warn, "Menu: the framework did not create a standalone window; the quick picker stays a menu page.");
		}
		// The tree titles follow the interface language. The framework splits the
		// path on '/', so a translated title must not contain one.
		const auto sectionPath = [](const char* a_item) {
			std::string root(I18n::Tr("Skyrim Outfit System"));
			std::string item(I18n::Tr(a_item));
			std::replace(root.begin(), root.end(), '/', '-');
			std::replace(item.begin(), item.end(), '/', '-');
			return root + "/" + item;
		};
		const auto quickPath = sectionPath("Quick picker");
		const auto outfitPath = sectionPath("Outfits");
		const auto actorsPath = sectionPath("Actors");
		const auto settingsPath = sectionPath("Settings");
		MenuBridge::AddSectionItem(quickPath.c_str(), RenderQuickPickerSectionThunk);
		MenuBridge::AddSectionItem(outfitPath.c_str(), RenderOutfitsThunk);
		MenuBridge::AddSectionItem(actorsPath.c_str(), RenderActorsThunk);
		MenuBridge::AddSectionItem(settingsPath.c_str(), RenderSettingsThunk);
		g_registered.store(true, std::memory_order_release);
		LOG(info, "Menu: sections registered (ImGui host: {}).", ImGuiApi::HostName());
		return true;
	}

	bool Registered()
	{
		return g_registered.load(std::memory_order_acquire);
	}

	void InvalidateBrowser()
	{
		g_filterSignature.clear();
		g_quickSignature.clear();
		g_outfitNamesCacheRevision = ~std::uint64_t{ 0 };
		Editor::Invalidate();
	}

	bool QuickPickerVisible()
	{
		return g_quickWindow && g_quickWindow->IsOpen.load();
	}

	void ShowQuickPicker(bool a_show)
	{
		if (!g_registered.load(std::memory_order_acquire) && !Register()) {
			return;
		}
		if (!g_quickWindow) {
			return;
		}
		ClearPadState();
		g_quickWindow->IsOpen = a_show;
		if (a_show) {
			// The actual list refresh, focus restore and scroll happen on the
			// render thread in RenderQuickPickerBody: this function may be called
			// by Papyrus or by the input thread.
			g_quickRefreshPending.store(true, std::memory_order_release);
			// Re-apply the stored rectangle on every open.
			g_quickGeometryForce.store(true, std::memory_order_release);
			const auto& settings = Settings::Get();
			LOG(debug, "Quick picker: opening at {},{} with {}x{} ({}).",
				settings.quickPickerX, settings.quickPickerY,
				settings.quickPickerWidth, settings.quickPickerHeight,
				settings.quickPickerX < 0 ? "centred" : "restored from the ini");
		}
	}

	void ToggleQuickPicker()
	{
		if (!g_registered.load(std::memory_order_acquire) && !Register()) {
			return;
		}
		if (!g_quickWindow) {
			return;
		}
		const bool open = g_quickWindow->IsOpen.load();
		ShowQuickPicker(!open);
	}

	bool TextInputActive()
	{
		if (!g_registered.load(std::memory_order_acquire)) {
			return false;
		}
		const auto context = ImGuiApi::Get().GetCurrentContext;
		return context && context() != nullptr && ImGuiApi::Get().IsAnyItemActive && ImGuiApi::Get().IsAnyItemActive();
	}
}
