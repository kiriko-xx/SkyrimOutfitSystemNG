#include "MenuUI.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <set>
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
#include "core/EspOutfits.h"
#include "core/GoldenItems.h"
#include "core/OutfitService.h"
#include "core/PluginScan.h"
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
		// 1.5.55: bulk delete. Ticking the box turns every row into a checkbox and
		// the count of ticked presets is shown above the list, so the user sees how
		// many presets a confirm is about to remove before pressing it.
		bool                      g_bulkMode{ false };
		std::set<std::string>     g_bulkSelected;
		bool                      g_bulkConfirm{ false };
		std::string               g_bulkStatus;
		// Result line of the last OutfitSystemData.json import / export.
		std::string               g_dataStatus;
		// Which actor the browser and the editor act on; 0 means the player.
		// The old MCM had the same "select the actor you are editing" control.
		RE::FormID                g_targetActor{ 0 };
		RE::FormID                g_pendingActorRemove{ 0 };

		// ---- golden items page state ----------------------------------------
		// The list is grouped by plugin, so the right hand editor always works
		// inside one plugin: adding a single piece to the selected group fixes
		// the plugin and only asks for the local form id.
		std::string g_goldenSelected;
		// 1.5.45: a single piece is picked out of a list instead of being typed,
		// so the page stores which plugin's piece list is open and two search
		// boxes: one for the picker above, one for the editor on the right.
		std::string g_goldenPiecePlugin;
		char        g_goldenPieceFilter[128]{};
		char        g_goldenEditorFilter[128]{};
		char        g_goldenFilter[128]{};
		std::string g_goldenStatus;
		// True while the golden item store differs from the file on disk. Every
		// edit used to write the file on the spot, behind the user's back
		// (1.5.43: the Export button is the only thing that saves).
		bool        g_goldenDirty{ false };
		// The framework has no popup windows, so the plugin chooser of both pages
		// is an inline list: 1 = every piece of a plugin, 2 = the plugin whose
		// pieces are listed so one of them can be picked.
		int                     g_goldenPluginPicker{ 0 };
		bool                    g_espOutfitPicker{ false };
		char                    g_espOutfitFilter[128]{};
		std::string             g_espOutfitStatus;
		EspOutfits::Preview     g_espOutfitPreview;

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

			// Renames and deletes elsewhere invalidate ticked rows. The list is the
			// only place a tick can come from, so pruning here (once per rebuild,
			// not once per frame) is enough and keeps the count honest.
			if (!g_bulkSelected.empty()) {
				for (auto it = g_bulkSelected.begin(); it != g_bulkSelected.end();) {
					if (!inner.has_outfit(it->c_str())) {
						it = g_bulkSelected.erase(it);
					} else {
						++it;
					}
				}
				if (g_bulkSelected.empty()) {
					g_bulkConfirm = false;
				}
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

			IG::Text(I18n::Tr("Change outfit"));
			IG::SameLine();
			IG::TextDisabled(I18n::Tr("%zu outfits"), outfit_service_get_singleton_ptr()->inner().outfit_count());

			IG::SetNextItemWidth(300.0f);
			if (IG::InputTextWithHint("##quicksearch", "Search...", g_quickSearch, sizeof(g_quickSearch))) {
				g_quickSignature.clear();
				g_quickFocus = 0;
			}
			IG::SameLine();
			if (IG::Checkbox(I18n::TrId("Favourites"), &g_quickFavoritesOnly)) {
				g_quickSignature.clear();
				g_quickFocus = 0;
			}
			IG::SameLine();
			IG::SetNextItemWidth(140.0f);
			if (IG::SliderInt(I18n::TrId("Page size"), &settings.quickPickerPageSize, 5, 500)) {
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
					IG::TextDisabled(I18n::Tr("No outfit matches the filter."));
				}
			}
			IG::EndChild();

			IG::Separator();
			if (IG::Button(I18n::TrId("Random"))) {
				QuickRandom(player);
			}
			IG::SameLine();
			if (IG::Button(I18n::TrId("Remove outfit"))) {
				QuickRemoveOutfit(player);
			}
			if (IG::IsItemHovered()) {
				IG::SetTooltip(I18n::Tr("Removes the outfit from the player and shows the real armour again (X)."));
			}
			IG::SameLine();
			if (IG::Button(I18n::TrId("Close"))) {
				CloseQuickPicker();
			}

			IG::TextDisabled(I18n::Tr("W/S: select   A/D: page   E: wear   R: random   X: remove   Esc: close"));
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
					IG::TextDisabled(I18n::Tr("The menu framework does not expose the window functions; the quick picker cannot open."));
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

			IG::Text(I18n::Tr("Outfits: %zu"), outfitCount);
			IG::SameLine();
			IG::TextDisabled(I18n::Tr("(double-click a name to apply it to %s)"), TargetName());

			IG::SetNextItemWidth(320.0f);
			if (IG::InputTextWithHint("##search", "Search by name...", g_search, sizeof(g_search))) {
				g_page = 1;
			}
			IG::SameLine();
			if (IG::Checkbox(I18n::TrId("Favourites only"), &g_searchFavoritesOnly)) {
				g_page = 1;
			}
			IG::SameLine();
			if (IG::Checkbox(I18n::T("Bulk delete", "批量删除"), &g_bulkMode)) {
				g_bulkConfirm = false;
				if (!g_bulkMode) {
					g_bulkSelected.clear();
					g_bulkStatus.clear();
				}
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
			IG::Text(I18n::Tr("page %d / %d"), g_page, pageCount);
			IG::SameLine();
			if (IG::SmallButton(">")) { g_page = std::min(pageCount, g_page + 1); }
			IG::SameLine();
			if (IG::SmallButton(">|")) { g_page = pageCount; }
			IG::SameLine();
			// The page size is stored in the ini, so it does not reset every time
			// the menu is opened.
			IG::SetNextItemWidth(140.0f);
			if (IG::SliderInt(I18n::TrId("Page size"), &settings.outfitPageSize, 5, 500)) {
				settings.outfitPageSize = std::clamp(settings.outfitPageSize, 5, 500);
				g_page = 1;
				g_filterSignature.clear();
				SaveSettingsThrottled();
			}
			IG::SameLine();
			if (IG::BeginCombo(I18n::TrId("Apply to"), TargetName())) {
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
				IG::TextWrapped(I18n::Tr("Delete outfit \"%s\"? This also removes it from every actor and state assignment."), g_pendingDelete.c_str());
				if (IG::Button(I18n::TrId("Delete"))) {
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
				if (IG::Button(I18n::TrId("Cancel"))) {
					g_pendingDelete.clear();
				}
				IG::Separator();
			}

			// A delete empties g_filtered inside this same frame (the handler below
			// calls g_filtered.clear()) while g_page still points at the page the
			// list had before it shrank. end - begin then wraps to SIZE_MAX and
			// vector::assign throws std::length_error("vector too long"), which
			// escaped the menu render callback and closed the game (m10183, crash
			// 2026-11-21-05-13-54, KERNELBASE raise from inside RenderOutfits).
			// Not const: the bulk delete below empties the list inside this frame and
			// has to collapse the range, exactly like the single delete above.
			std::size_t total = g_filtered.size();
			std::size_t begin = std::min(total, static_cast<std::size_t>(std::max(0, (g_page - 1) * pageSize)));
			std::size_t end = std::min(total, begin + static_cast<std::size_t>(pageSize));

			// 1.5.55: bulk delete. The tick count is drawn from g_bulkSelected, so it
			// is always the number of presets the confirm button will remove.
			if (g_bulkMode) {
				IG::Text(I18n::T("%zu preset(s) ticked", "已勾选 %zu 套幻化"), g_bulkSelected.size());
				IG::SameLine();
				if (IG::SmallButton(I18n::T("Tick this page", "本页全选"))) {
					for (std::size_t i = begin; i < end; ++i) {
						g_bulkSelected.insert(g_filtered[i]);
					}
					g_bulkConfirm = false;
				}
				IG::SameLine();
				if (IG::SmallButton(I18n::T("Untick this page", "本页取消"))) {
					for (std::size_t i = begin; i < end; ++i) {
						g_bulkSelected.erase(g_filtered[i]);
					}
					g_bulkConfirm = false;
				}
				IG::SameLine();
				if (IG::SmallButton(I18n::T("Tick every match", "全部筛选结果"))) {
					g_bulkSelected.insert(g_filtered.begin(), g_filtered.end());
					g_bulkConfirm = false;
				}
				IG::SameLine();
				if (IG::SmallButton(I18n::T("Untick all", "清空勾选"))) {
					g_bulkSelected.clear();
					g_bulkConfirm = false;
				}
				IG::SameLine();
				if (g_bulkSelected.empty()) {
					IG::TextDisabled(I18n::T("Tick presets in the list, then confirm the delete.", "先在下方列表勾选幻化，再确认删除。"));
				} else if (IG::Button(I18n::T("Delete the ticked presets", "删除勾选的幻化"))) {
					g_bulkConfirm = true;
				}
				if (g_bulkConfirm && !g_bulkSelected.empty()) {
					IG::TextWrapped(I18n::T("Delete %zu preset(s)? This also removes them from every actor and state assignment.",
										"删除勾选的 %zu 套幻化？这也会把它们从所有角色与状态指派中移除。"),
						g_bulkSelected.size());
					if (IG::Button(I18n::T("Yes, delete them", "确认删除"))) {
						std::vector<std::string> names(g_bulkSelected.begin(), g_bulkSelected.end());
						const auto removed = outfit_service_get_mut_singleton_ptr()->inner().delete_outfits(names);
						SKSE::log::info("Outfits: bulk delete removed {} of {} ticked preset(s).", removed, names.size());
						char text[160]{};
						std::snprintf(text, sizeof(text), I18n::T("Deleted %zu preset(s).", "已删除 %zu 套幻化。"), removed);
						g_bulkStatus = text;
						if (g_bulkSelected.count(g_selected) != 0) {
							g_selected.clear();
						}
						g_bulkSelected.clear();
						g_bulkConfirm = false;
						g_pendingDelete.clear();
						g_filterSignature.clear();
						g_quickSignature.clear();
						g_filtered.clear();
						total = 0;
						begin = 0;
						end = 0;
						CreateRefreshAllArmorsTask();
					}
					IG::SameLine();
					if (IG::Button(I18n::T("Cancel", "取消"))) {
						g_bulkConfirm = false;
					}
				}
				if (!g_bulkStatus.empty()) {
					IG::TextUnformatted(g_bulkStatus.c_str());
				}
				IG::Separator();
			}

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

					if (g_bulkMode) {
						bool ticked = g_bulkSelected.count(name) != 0;
						if (IG::Checkbox("##bulk", &ticked)) {
							if (ticked) {
								g_bulkSelected.insert(name);
							} else {
								g_bulkSelected.erase(name);
							}
							g_bulkConfirm = false;
						}
						IG::SameLine();
					}

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
						IG::SetTooltip(I18n::Tr("%zu armour pieces\nWear applies it to the actor chosen in \"Apply to\"."), armorCount);
					}
					if (isSelected && g_scrollToSelected) {
						IG::SetScrollHereY(0.5f);
						g_scrollToSelected = false;
					}

					IG::SameLine(buttonX);
					if (IG::SmallButton(I18n::TrId("Wear"))) {
						ApplyTo(TargetActor(), name);
					}
					IG::SameLine();
					if (IG::SmallButton(isFavorite ? "*" : "-")) {
						outfit_service_get_mut_singleton_ptr()->inner().set_favorite(name.c_str(), !isFavorite);
						g_filterSignature.clear();
					}
					IG::SameLine();
					if (IG::SmallButton(I18n::TrId("Delete"))) {
						g_pendingDelete = name;
					}

					IG::PopID();
				}
				if (g_filtered.empty()) {
					IG::TextDisabled(I18n::Tr("No outfits yet - wear some armour and use 'Create from worn' below."));
				}
			}
			IG::EndChild();

			IG::SameLine();
			if (IG::BeginChild("##outfiteditor", ImVec2(0.0f, 470.0f), true, ImGuiWindowFlags_HorizontalScrollbar)) {
				if (g_selected.empty()) {
					IG::TextDisabled(I18n::Tr("Select an outfit on the left to edit its armour pieces and slot policies."));
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
			if (IG::Button(I18n::TrId("Create from worn"))) {
				if (nameTooLong) {
					RE::SendHUDMessage::ShowHUDMessage(I18n::T("That outfit name is too long.", "幻化名称过长。"), nullptr, false);
				} else if (Features::CreateOutfitFromWorn(RE::PlayerCharacter::GetSingleton(), g_newName)) {
					g_selected = g_newName;
					g_newName[0] = 0;
					g_filterSignature.clear();
				}
			}
			IG::SameLine();
			if (IG::Button(I18n::TrId("Create empty"))) {
				if (nameTooLong) {
					RE::SendHUDMessage::ShowHUDMessage(I18n::T("That outfit name is too long.", "幻化名称过长。"), nullptr, false);
				} else if (g_newName[0] != 0 && outfit_service_get_mut_singleton_ptr()->inner().add_outfit(g_newName)) {
					g_selected = g_newName;
					g_newName[0] = 0;
					g_filterSignature.clear();
				}
			}
			IG::SameLine();
			if (IG::Button(I18n::TrId("Copy"))) {
				if (g_selected.empty()) {
					RE::SendHUDMessage::ShowHUDMessage(I18n::T("Select an outfit on the left first.", "请先在左侧选中一套幻化。"), nullptr, false);
				} else if (OutfitClipboard::Copy(g_selected)) {
					RE::SendHUDMessage::ShowHUDMessage(I18n::T("Outfit copied to the clipboard.", "幻化已复制到剪贴板。"), nullptr, false);
				} else {
					RE::SendHUDMessage::ShowHUDMessage(I18n::T("Could not copy the outfit to the clipboard.", "无法把幻化复制到剪贴板。"), nullptr, false);
				}
			}
			IG::SameLine();
			if (IG::Button(I18n::TrId("Paste"))) {
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
			IG::TextDisabled(I18n::Tr("Max %zu bytes per name."), maxNameLength);

			if (!g_selected.empty()) {
				IG::SetNextItemWidth(240.0f);
				IG::InputText("##rename", g_renameName, sizeof(g_renameName));
				IG::SameLine();
				if (IG::Button(I18n::TrId("Rename"))) {
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
			IG::SeparatorText(I18n::T("Generate outfits from a plugin", "从 ESP 生成幻化"));
			IG::TextWrapped("%s", I18n::T("Select a plugin to preview its outfits. Uncertain matches are not created.", "选择 ESP 后先预览幻化预设，无法确定的搭配不会生成。"));
			IG::TextDisabled(I18n::T("Only ordinary clothing is taken: FX-only records, records with no readable name, and records with no model are left out.", "只收录正常服装：纯 FX 特效、无可读名称或无模型的记录不算在内。"));
			if (IG::Button(I18n::T("Choose a plugin...", "选择 ESP..."))) {
				g_espOutfitPicker = !g_espOutfitPicker;
				g_espOutfitFilter[0] = 0;
			}
			IG::SameLine();
			bool espFavorite = settings.espOutfitAutoFavorite;
			if (IG::Checkbox(I18n::T("Mark generated outfits as favourites", "生成的幻化自动收藏"), &espFavorite)) {
				settings.espOutfitAutoFavorite = espFavorite;
				SaveSettingsThrottled();
			}
			if (g_espOutfitPicker) {
				IG::SetNextItemWidth(320.0f);
				IG::InputTextWithHint("##esppluginfilter", I18n::T("Search plugins...", "搜索 ESP..."), g_espOutfitFilter, sizeof(g_espOutfitFilter));
				if (IG::BeginChild("##espplugins", ImVec2(0.0f, 240.0f), true, ImGuiWindowFlags_HorizontalScrollbar)) {
					const auto  plugins = EspOutfits::PluginsWithArmor();
					const auto  needle = Lower(g_espOutfitFilter);
					std::size_t shown = 0;
					// Reversed: the plugin loaded last is the one the user just installed,
					// so it sits on top of the list.
					for (auto it = plugins.rbegin(); it != plugins.rend(); ++it) {
						if (!ContainsNoCase(it->name, needle)) {
							continue;
						}
						++shown;
						char label[320]{};
						if (it->armors > it->outfitArmors) {
							std::snprintf(label, sizeof(label), I18n::T("%s  (%zu piece(s), %zu skipped as not clothing)", "%s（%zu 件，%zu 项非服装已跳过）"), it->name.c_str(), it->outfitArmors, it->armors - it->outfitArmors);
						} else {
							std::snprintf(label, sizeof(label), I18n::T("%s  (%zu piece(s))", "%s（%zu 件）"), it->name.c_str(), it->outfitArmors);
						}
						IG::PushID(static_cast<int>(it->order));
						if (IG::Selectable(label)) {
							g_espOutfitPreview = EspOutfits::PreviewFromPlugin(it->name);
							g_espOutfitStatus.clear();
							g_espOutfitPicker = false;
						}
						IG::PopID();
					}
					if (shown == 0) {
						IG::TextDisabled(I18n::T("No plugin matches that search.", "没有匹配的 ESP。"));
					}
				}
				IG::EndChild();
			}
			if (!g_espOutfitPreview.plugin.empty()) {
				const auto& preview = g_espOutfitPreview;
				IG::TextWrapped(I18n::T("%s: %zu proposed, %zu pieces pending, %zu non-clothing records filtered.", "%s：拟生成 %zu 个预设，%zu 件待判断，已过滤 %zu 项非服装。"),
					preview.plugin.c_str(), preview.outfits.size(), preview.unresolved.size(), preview.notClothing);
				if (preview.truncated) {
					IG::TextWrapped("%s", I18n::T("The preset limit was reached; some groups were left pending.", "已达到预设数量上限，部分款式留待判断。"));
				}
				if (IG::BeginChild("##espoutfitpreview", ImVec2(0.0f, 220.0f), true, ImGuiWindowFlags_HorizontalScrollbar)) {
					// The rule that left a piece out is printed next to it, so one screenshot
					// is enough to see why an outfit was not generated.
					const auto reasonLabel = [](const std::string& why) -> const char* {
						if (why == "no-supported-family") return I18n::T("no shared style name", "没有同款式的名称前缀");
						if (why == "overlapping-slot-masks") return I18n::T("shares a slot with another piece", "与其他件占用同一槽位");
						if (why == "non-unique-difference") return I18n::T("several pieces on one slot look the same", "同槽多件无法区分");
						if (why == "unmatched-difference") return I18n::T("the variant marks do not line up", "同款各槽的差目标号对不上");
						if (why == "no-complementary-slots") return I18n::T("every piece covers the same slot", "所有件都占同一槽位");
						if (why == "no-complete-outfit") return I18n::T("not enough pieces for a full outfit", "凑不成成套件");
						if (why == "incomplete-slots") return I18n::T("this style can never fill its slots", "这套的槽位凑不齐");
						if (why == "same-slot-alternative") return I18n::T("another option of a slot already filled", "同一槽位已有别的件，这件未采用");
						if (why == "variant-limit") return I18n::T("too many variants on one slot", "同一槽位的变体过多");
						if (why == "preset-limit") return I18n::T("preset limit reached", "已达预设数量上限");
						if (why == "duplicate-preset-name") return I18n::T("a preset with that name already exists", "已有同名预设");
						if (why == "unsupported-preset-name") return I18n::T("the name cannot be used for a preset", "名称不能用作预设名");
						return I18n::T("not enough evidence", "证据不足");
					};
					const auto drawPiece = [&preview, &reasonLabel](std::size_t index, bool withReason) {
						const auto& piece = preview.pieces[index];
						const auto slots = EspOutfits::SlotNumbers(piece.mask);
						IG::Text(I18n::T("%s (slots %s) [%08X]", "%s（槽位 %s）[%08X]"),
							(piece.name.empty() ? piece.editorId : piece.name).c_str(), slots.c_str(), piece.formId);
						if (withReason && index < preview.reasons.size()) {
							IG::SameLine();
							const bool named = index < preview.families.size() && !preview.families[index].empty();
							if (named) {
								IG::TextDisabled(I18n::T("in '%s' (%s)", "属于「%s」：%s"), preview.families[index].c_str(),
									reasonLabel(preview.reasons[index]));
							} else {
								IG::TextDisabled("(%s)", reasonLabel(preview.reasons[index]));
							}
						}
					};
					for (std::size_t i = 0; i < preview.outfits.size(); ++i) {
						IG::PushID(static_cast<int>(i));
						const auto& outfit = preview.outfits[i];
						const auto label = outfit.existing ? outfit.name + I18n::T(" (already exists; will skip)", "（已存在，将跳过）") : outfit.name;
						if (IG::TreeNode(label.c_str())) {
							for (const auto index : preview.outfits[i].pieces) {
								drawPiece(index, false);
							}
							IG::TreePop();
						}
						IG::PopID();
					}
					if (!preview.unresolved.empty() && IG::TreeNode(I18n::T("Pending pieces (will not be created)", "待判断的装备（不会生成）"))) {
						for (const auto index : preview.unresolved) {
							drawPiece(index, true);
						}
						IG::TreePop();
					}
				}
				IG::EndChild();
				if (!preview.outfits.empty()) {
					if (IG::Button(I18n::T("Create previewed outfits", "确认生成预览中的幻化"))) {
						const auto result = EspOutfits::ApplyPreview(preview, settings.espOutfitAutoFavorite);
						char status[320]{};
						std::snprintf(status, sizeof(status), I18n::T("Created %zu outfits; skipped %zu. Existing presets were not replaced.", "已生成 %zu 个幻化，跳过 %zu 个；没有覆盖已有预设。"), result.names.size(), result.skipped);
						g_espOutfitStatus = status;
						g_espOutfitPreview = {};
						if (!result.names.empty()) {
							FocusOutfitInList(result.names.front());
							g_quickSignature.clear();
							CreateRefreshAllArmorsTask();
						}
					}
					IG::SameLine();
				}
				if (IG::Button(I18n::T("Close outfit preview", "关闭幻化预览"))) {
					g_espOutfitPreview = {};
				}
			}
			if (!g_espOutfitStatus.empty()) {
				IG::TextWrapped("%s", g_espOutfitStatus.c_str());
			}

			IG::Separator();
			IG::TextDisabled(I18n::Tr("Plugin enabled: %s | Random hotkey: %s"), outfit_service_get_singleton_ptr()->inner().enabled_c() ? I18n::T("yes", "是") : I18n::T("no", "否"), settings.randomOutfitHotkey.c_str());
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

				if (IG::BeginCombo(I18n::TrId("Outfit"), currentName.empty() ? "<none>" : currentName.c_str())) {
					if (IG::Selectable(I18n::TrId("<none>"), currentName.empty())) {
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

				if (IG::Checkbox(I18n::TrId("Location/state based switching"), &autoSwitch)) {
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

				if (IG::TreeNode(I18n::TrId("State outfits"))) {
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
							if (IG::Selectable(I18n::TrId("<inherit>"), stateName.empty())) {
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
				if (IG::SmallButton(I18n::TrId("Refresh"))) {
					Features::RefreshActor(actor);
				}
				IG::SameLine();
				if (IG::SmallButton(I18n::T("Remove", "移除"))) {
					g_pendingActorRemove = a_formId;
				}

				IG::TreePop();
			}

			IG::PopID();
		}

		void RenderActors()
		{
			if (auto* player = RE::PlayerCharacter::GetSingleton(); player && IG::Button(I18n::TrId("Add player"))) {
				outfit_service_get_mut_singleton_ptr()->inner().add_actor(player->GetFormID());
			}
			IG::SameLine();
			if (IG::BeginCombo("##addnearby", "Add a nearby actor...")) {
				const auto nearby = Features::ActorsNearPlayer();
				if (nearby.empty()) {
					IG::TextDisabled(I18n::Tr("No other actor in this cell."));
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
			if (IG::Button(I18n::TrId("Refresh all appearances"))) {
				CreateRefreshAllArmorsTask();
			}
			IG::Separator();

			if (g_pendingActorRemove != 0) {
				if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(g_pendingActorRemove)) {
					const char* name = actor->GetDisplayFullName();
					IG::TextWrapped(I18n::Tr("Stop managing %s? The player cannot be removed."), name ? name : I18n::T("<not loaded>", "<未加载>"));
				} else {
					IG::TextWrapped(I18n::Tr("Stop managing that actor?"));
				}
				if (IG::Button(I18n::TrId("Remove"))) {
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
				if (IG::Button(I18n::TrId("Cancel"))) {
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
				IG::TextDisabled(I18n::Tr("No actors configured yet."));
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

			IG::SeparatorText(I18n::Tr("General"));
			// The runtime state lives in OutfitService (it is persisted in the co-save),
			// so derive the checkbox from it instead of the stale Settings mirror.
			bool enabled = outfit_service_get_singleton_ptr()->inner().enabled_c();
			if (IG::Checkbox(I18n::TrId("Enable outfit overrides"), &enabled)) {
				settings.enabled = enabled;
				outfit_service_get_mut_singleton_ptr()->inner().set_enabled(enabled);
				CreateRefreshAllArmorsTask();
				Settings::Get().Save();
			}
			bool quickslots = settings.quickslotsEnabled;
			if (IG::Checkbox(I18n::TrId("Give the player the \"Change Outfit\" power"), &quickslots)) {
				settings.quickslotsEnabled = quickslots;
				Features::ApplyQuickslotSpell();
				Settings::Get().Save();
			}
			IG::TextDisabled(I18n::Tr("Version %s"), SKYRIMOUTFITSYSTEMNG_VERSION_VERSTRING);
			IG::TextDisabled("Settings are stored in Data\\SKSE\\Plugins\\SkyrimOutfitSystemNG.ini");

			IG::SeparatorText(I18n::Tr("Outfit data"));
			IG::TextDisabled("Data\\SKSE\\Plugins\\OutfitSystemData.json - the same file the original mod reads and writes.");
			if (IG::Button(I18n::TrId("Export outfits to OutfitSystemData.json"))) {
				g_dataStatus.clear();
				std::string message;
				const bool ok = Features::ExportOutfitsToJson(message);
				g_dataStatus = message;
				RE::SendHUDMessage::ShowHUDMessage(message.c_str(), nullptr, false);
				SKSE::log::info("Menu: export to OutfitSystemData.json {}.", ok ? "succeeded" : "failed");
			}
			IG::SameLine();
			if (IG::Button(I18n::TrId("Import outfits from OutfitSystemData.json"))) {
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

			IG::SeparatorText(I18n::T("Outfit generation", "套装生成"));
			{
				bool espFavorite = settings.espOutfitAutoFavorite;
				if (IG::Checkbox(I18n::T("Mark outfits generated from a plugin as favourites", "从 ESP 生成的幻化自动收藏"), &espFavorite)) {
					settings.espOutfitAutoFavorite = espFavorite;
					Settings::Get().Save();
				}
				IG::TextDisabled(I18n::T("Generated outfits take ordinary clothing only: FX-only records, records with no readable name, and records with no model are skipped. Golden items are not filtered.", "生成的幻化预设只收录正常服装：跳过纯 FX 特效、无可读名称或无模型的记录；金件不做这个过滤。"));
			}

			IG::SeparatorText(I18n::Tr("Language"));
			{
				// Every file in Data/interface/translations becomes a choice, so a
				// translator can add a language without touching the plugin.
				const std::string mode(I18n::ModeName());
				const bool        isAuto = mode == "auto";
				const char*       preview = isAuto ? I18n::Tr("Auto (game language)") : I18n::LabelForCode(I18n::CurrentCode());
				if (IG::BeginCombo(I18n::TrId("Language"), preview)) {
					if (IG::Selectable(I18n::TrId("Auto (game language)"), isAuto)) {
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
				if (IG::Button(I18n::TrId("Reload the language files"))) {
					I18n::ReloadFile();
					RE::SendHUDMessage::ShowHUDMessage(I18n::T("Translation file reloaded.", "语言文件已重新加载。"), nullptr, false);
				}
				IG::SameLine();
				IG::TextDisabled(I18n::Tr("Translation file: %s"), I18n::FileStatus());
				IG::TextDisabled("Data\\interface\\translations\\SkyrimOutfitSystemNG_<language>.txt - one file per language.");
				IG::TextDisabled(I18n::Tr("Menu titles update after a game restart (the framework caches them)."));
			}

			IG::SeparatorText(I18n::Tr("Mod compatibility"));
			const auto& compat = Compat::GetStatus();
			bool andEnabled = settings.compatAdvancedNudityDetection;
			if (IG::Checkbox(I18n::TrId("Advanced Nudity Detection sees the outfit"), &andEnabled)) {
				settings.compatAdvancedNudityDetection = andEnabled;
				Settings::Get().Save();
			}
			if (!compat.andDetected) {
				IG::TextDisabled(I18n::Tr("Advanced Nudity Detection: not installed."));
			} else if (!compat.andHooked) {
				IG::TextDisabled(I18n::Tr("Advanced Nudity Detection %s: its nudity factions were not found, detection keeps the vanilla behaviour."),
					compat.andVersion.empty() ? "" : compat.andVersion.c_str());
			} else {
				IG::TextDisabled(I18n::Tr("Advanced Nudity Detection %s: nudity, topless and bottomless follow the outfit."),
					compat.andVersion.empty() ? "" : compat.andVersion.c_str());
			}
			IG::TextDisabled(I18n::Tr("An empty outfit counts as bare skin."));
			bool padEnabled = settings.compatProceduralArmorDamage;
			if (IG::Checkbox(I18n::TrId("Procedural Armor Damage tears the outfit"), &padEnabled)) {
				settings.compatProceduralArmorDamage = padEnabled;
				Settings::Get().Save();
			}
			if (!compat.padDetected) {
				IG::TextDisabled(I18n::Tr("Procedural Armor Damage: not installed."));
			} else {
				IG::TextDisabled(I18n::Tr("Procedural Armor Damage %s: damage is stored on the real armour worn in the same slot."),
					compat.padVersion.empty() ? "" : compat.padVersion.c_str());
			}
			IG::TextDisabled(I18n::Tr("Pieces shown over a slot with no real armour underneath cannot be damaged."));
			bool smpReset = settings.autoResetSmp;
			if (IG::Checkbox(I18n::TrId("Reset SMP physics when the outfit changes"), &smpReset)) {
				settings.autoResetSmp = smpReset;
				Settings::Get().Save();
			}
			IG::TextDisabled(I18n::Tr("Only the actor whose outfit changed is reset."));
			if (!SMP::IsAvailable()) {
				IG::TextDisabled(I18n::Tr("Faster HDT-SMP (DynamicHDT): not detected; this switch has no effect."));
			} else {
				IG::TextDisabled(I18n::Tr("Faster HDT-SMP (DynamicHDT): detected."));
			}
			bool sceneOutfits = settings.sceneOutfitSwitching;
			if (IG::Checkbox(I18n::TrId("Wear <current outfit>_sex during SexLab / OStim scenes"), &sceneOutfits)) {
				settings.sceneOutfitSwitching = sceneOutfits;
				Settings::Get().Save();
			}
			IG::TextDisabled(I18n::Tr("If the current outfit already ends with _sex it is kept unchanged."));
			IG::TextDisabled(I18n::Tr("The previous outfit is restored when the scene ends."));
			if (IG::Button(I18n::TrId("Log slot report"))) {
				Compat::Pad::Diagnose(RE::PlayerCharacter::GetSingleton());
				RE::SendHUDMessage::ShowHUDMessage(I18n::T("Slot report written to OutfitSystemNG.log", "槽位报告已写入 OutfitSystemNG.log"), nullptr, false);
			}
			IG::SameLine();
			IG::TextDisabled(I18n::Tr("Lists every slot: what the outfit shows, what is really worn."));

			IG::SeparatorText(I18n::Tr("Hotkeys"));

			// Hotkeys are recorded, not typed. Every row has one button per device: a
			// single shared recorder could not tell a controller button from a
			// keyboard key, and the pad "A" that recorded a chord also confirmed the
			// menu item under it.

			// The row whose capture window is open; -1 when none.
			static int g_hotkeyRow = -1;

			auto applyHotkey = [&settings](Hotkeys::Slot a_slot, const std::string& a_text) {
				switch (a_slot) {
				case Hotkeys::Slot::kRandomOutfit:
					settings.randomOutfitHotkey = a_text;
					break;
				case Hotkeys::Slot::kQuickPicker:
					settings.quickPickerHotkey = a_text;
					break;
				case Hotkeys::Slot::kQuickPickerKey:
					settings.quickPickerKey = a_text;
					break;
				}
				Settings::Get().Save();
				Hotkeys::Reload();
			};

			{
				Hotkeys::Slot capturedSlot{};
				std::string   capturedText;
				if (Hotkeys::TakeCaptureResult(capturedSlot, capturedText)) {
					applyHotkey(capturedSlot, capturedText);
					g_hotkeyRow = -1;
				}
			}

			auto hotkeyRow = [&applyHotkey](Hotkeys::Slot a_slot, const char* a_label, const std::string& a_value) {
				// Two rows carry the same label in translation, so the ids come from
				// the slot rather than from the button text.
				IG::PushID(static_cast<int>(a_slot));
				IG::TextUnformatted(a_label);
				IG::SameLine(280.0f);
				const auto shown = a_value.empty() ? std::string(I18n::T("Not set", "未设置")) : a_value;
				IG::TextUnformatted(shown.c_str());
				IG::SameLine();
				if (g_hotkeyRow == static_cast<int>(a_slot)) {
					IG::TextDisabled(I18n::Tr("recording..."));
				} else {
					if (IG::Button(I18n::TrId("Record keyboard"))) {
						Hotkeys::BeginCapture(a_slot, Hotkeys::Input::kKeyboard);
						g_hotkeyRow = static_cast<int>(a_slot);
					}
					IG::SameLine();
					if (IG::Button(I18n::TrId("Record controller"))) {
						Hotkeys::BeginCapture(a_slot, Hotkeys::Input::kGamepad);
						g_hotkeyRow = static_cast<int>(a_slot);
					}
					IG::SameLine();
					if (IG::SmallButton(I18n::TrId("Clear"))) {
						applyHotkey(a_slot, std::string{});
					}
				}
				IG::PopID();
			};

			hotkeyRow(Hotkeys::Slot::kRandomOutfit, I18n::Tr("Random outfit"), settings.randomOutfitHotkey);
			hotkeyRow(Hotkeys::Slot::kQuickPicker, I18n::Tr("Quick picker"), settings.quickPickerHotkey);
			{
				// The power key is not recorded: it is whatever the player bound to
				// Shout/Power in the game settings, keyboard or controller.
				const auto bound = Hotkeys::GamePowerKeyText();
				IG::TextUnformatted(I18n::Tr("Change Outfit power key"));
				IG::SameLine(280.0f);
				if (bound.empty()) {
					IG::TextDisabled(I18n::Tr("no key bound in the game settings"));
				} else {
					IG::Text(I18n::Tr("follows the game key binding: %s"), bound.c_str());
				}
			}
			IG::TextDisabled(I18n::Tr("This only fires while the \"Change Outfit\" power is the one selected in your magic menu."));

			// The recorder is its own window. It is raised every frame while it is
			// open, because the framework draws its own windows later in the frame
			// and would otherwise paint the settings page over it.
			if (Hotkeys::CaptureActive()) {
				IG::SetNextWindowPos(IG::MainViewportCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
				IG::SetNextWindowSize(ImVec2(520.0f, 0.0f), ImGuiCond_Always);
				IG::SetNextWindowFocus();
				bool open = true;
				IG::BeginWindow(I18n::T("Set a hotkey", "设置热键"), &open,
					ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
				const bool pad = Hotkeys::CaptureDevice() == Hotkeys::Input::kGamepad;
				IG::TextWrapped(pad ? I18n::Tr("Recording the controller: hold up to four buttons, then let go of all of them.")
									: I18n::Tr("Recording the keyboard and the mouse: hold up to four keys, then let go of all of them."));
				IG::Spacing();
				const auto typed = Hotkeys::CaptureText();
				if (typed.empty()) {
					IG::TextDisabled(I18n::Tr("Waiting for input..."));
				} else {
					IG::TextUnformatted(typed.c_str());
				}
				if (Hotkeys::CaptureOverflowed()) {
					IG::TextWrapped(I18n::Tr("Only the first four keys are used."));
				}
				IG::Spacing();
				IG::TextDisabled(I18n::Tr("Release every key when you are done; the chord is stored in the ini right away."));
				IG::TextDisabled(I18n::Tr("Press Esc to cancel."));
				IG::Spacing();
				if (IG::Button(I18n::TrId("Cancel"))) {
					Hotkeys::CancelCapture();
					g_hotkeyRow = -1;
				}
				IG::SameLine();
				if (IG::Button(I18n::TrId("Clear"))) {
					if (g_hotkeyRow >= 0) {
						applyHotkey(static_cast<Hotkeys::Slot>(g_hotkeyRow), std::string{});
					}
					Hotkeys::CancelCapture();
					g_hotkeyRow = -1;
				}
				IG::EndWindow();
				if (!open) {
					Hotkeys::CancelCapture();
					g_hotkeyRow = -1;
				}
			}


			IG::SeparatorText(I18n::Tr("Random outfit"));
			if (IG::Checkbox(I18n::TrId("Favourites only"), &settings.randomFavoritesOnly)) {
				Settings::Get().Save();
			}
			if (IG::Checkbox(I18n::TrId("Never pick the outfit that is already worn"), &settings.randomExcludeCurrent)) {
				Settings::Get().Save();
			}
			bool quickslotRandom = settings.quickslotRandom;
			if (IG::Checkbox(I18n::TrId("Make the \"Change Outfit\" power roll a random outfit"), &quickslotRandom)) {
				settings.quickslotRandom = quickslotRandom;
				Settings::Get().Save();
			}
			IG::TextDisabled(I18n::Tr("Off (default) opens the quick picker window instead."));
			if (IG::Button(I18n::TrId("Roll now"))) {
				const auto chosen = Features::PickRandomOutfit(RE::PlayerCharacter::GetSingleton(), settings.randomFavoritesOnly);
				const auto message = chosen.empty() ? std::string(I18n::T("No outfits available.", "没有可用幻化。")) : (std::string(I18n::T("Outfit: ", "幻化：")) + chosen);
				RE::SendHUDMessage::ShowHUDMessage(message.c_str(), nullptr, false);
			}

			IG::SeparatorText(I18n::Tr("Interface"));
			{
				bool layoutChanged = false;
				if (IG::SliderInt(I18n::TrId("List width"), &settings.outfitListWidth, 240, 1200)) {
					settings.outfitListWidth = std::clamp(settings.outfitListWidth, 240, 1200);
					layoutChanged = true;
				}
				if (IG::SliderInt(I18n::TrId("Outfit page size"), &settings.outfitPageSize, 5, 500)) {
					settings.outfitPageSize = std::clamp(settings.outfitPageSize, 5, 500);
					g_filterSignature.clear();
					layoutChanged = true;
				}
				if (IG::SliderInt(I18n::TrId("Quick picker page size"), &settings.quickPickerPageSize, 5, 500)) {
					settings.quickPickerPageSize = std::clamp(settings.quickPickerPageSize, 5, 500);
					layoutChanged = true;
				}
				if (layoutChanged) {
					SaveSettingsThrottled();
				}
				IG::TextDisabled(I18n::Tr("List width and page sizes are stored in the ini and shared by the quick picker."));
			}

			IG::SeparatorText(I18n::Tr("Diagnostics"));
			bool extraLogging = settings.extraLogging;
			if (IG::Checkbox(I18n::TrId("Verbose logging (takes effect on the next start)"), &extraLogging)) {
				settings.extraLogging = extraLogging;
				Settings::Get().Save();
			}
			IG::TextDisabled(I18n::Tr("ImGui host: %s"), ImGuiApi::HostName());
		}

		void UpdateTextInputState()
		{
			// ImGui is not thread-safe: sample the focused-widget state here on the
			// render thread and let the input sink read the cached value.
			Hotkeys::SetTextInputActive(MenuBridge::TextInputActive());
			// While a hotkey is being recorded the host must not see the keys that
			// are being recorded: a controller button that records a chord would
			// otherwise also activate whatever the menu has focused. The recorder
			// itself never reads ImGui input, it polls the devices.
			IG::SetMenuInputSuppressed(Hotkeys::CaptureActive() && MenuBridge::EventCallbackAvailable());
			Hotkeys::Tick();
		}

		// The framework reports its own menu lifecycle. A recording ends with the
		// menu: otherwise the host keeps withholding its input and the paused
		// menu cannot be used any more.
		void __stdcall OnFrameworkMenuEvent(int a_eventType)
		{
			if (a_eventType != MenuBridge::kEventMenuClosed) {
				return;
			}
			if (Hotkeys::CaptureActive()) {
				Hotkeys::CancelCapture();
			}
			IG::SetMenuInputSuppressed(false);
		}

		bool __stdcall OnFrameworkInput(RE::InputEvent* a_event)
		{
			if (!a_event) {
				return false;
			}

			// While a hotkey is being recorded the keys belong to the recorder only.
			// Returning true removes the event from the queue, which is what keeps
			// the menu (and its gamepad navigation) from acting on them.
			if (Hotkeys::CaptureActive()) {
				bool swallowed = false;
				for (auto* event = a_event; event; event = event->next) {
					if (event->AsButtonEvent()) {
						swallowed = true;
					}
				}
				if (swallowed) {
					return true;
				}
			}

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

		// ---- golden items panel ---------------------------------------------
		// A golden item is drawn on top of whatever the actor wears. The list is
		// grouped by plugin because that is the unit the user thinks in ("everything
		// from that armour pack") and the unit the JSON file stores.
		std::string HexLocalId(std::uint32_t a_localId)
		{
			char text[16]{};
			std::snprintf(text, sizeof(text), "%06X", a_localId & 0x00FFFFFFu);
			return text;
		}

		// "31, 32" - the numbers the creation kit and xEdit show, i.e. the zero
		// based slot index plus 30.
		std::string SlotNumbersFor(const RE::TESObjectARMO* a_armor)
		{
			const auto  mask = a_armor ? a_armor->GetSlotMask().underlying() : 0;
			std::string text;
			for (std::uint32_t bit = 0; bit < 32; ++bit) {
				if ((mask & (1u << bit)) == 0) {
					continue;
				}
				char number[8]{};
				std::snprintf(number, sizeof(number), text.empty() ? "%u" : ", %u", bit + 30);
				text += number;
			}
			return text.empty() ? std::string("?") : text;
		}

		// A list row the mouse can actually hit: the full width of what is
		// visible plus a little vertical padding. The plain two-argument
		// Selectable spans the content region of a scrolling child, which in this
		// framework can end up far narrower than the visible list, so most of a
		// row was not clickable.
		bool GoldenRow(const char* a_label, bool a_selected)
		{
			const float width = std::max(IG::ContentRegionAvail().x, IG::CalcTextSize(a_label).x + 32.0f);
			const float height = IG::CalcTextSize("A").y + 8.0f;
			return IG::Selectable(a_label, a_selected, 0, ImVec2(width, height));
		}

		// The local ids the store already holds for one plugin, so the piece list
		// can mark them. An empty list means the plugin is not a group yet.
		std::vector<std::uint32_t> AddedIdsOf(const std::vector<Golden::Group>& a_groups, const std::string& a_plugin)
		{
			const auto needle = Lower(a_plugin);
			for (const auto& group : a_groups) {
				if (Lower(group.plugin) == needle) {
					return group.localIds;
				}
			}
			return {};
		}

		// Every slot armour of one plugin as a clickable row; a click adds that
		// single piece to the plugin's golden group. 1.5.45 replaced the hand
		// typed local form id with this list: the plugin is picked first and the
		// piece is clicked out of it, so a wrong id can no longer be typed in.
		void RenderGoldenPieceList(const std::string& a_plugin, const std::vector<std::uint32_t>& a_added, int a_scopeId, float a_height, char* a_filter, std::size_t a_filterSize)
		{
			if (a_plugin.empty()) {
				return;
			}
			const auto armors = PluginScan::ArmorsOf(a_plugin);
			IG::PushID(a_scopeId);
			IG::SetNextItemWidth(280.0f);
			IG::InputTextWithHint("##goldenpiecefilter", I18n::T("Search pieces...", "搜索装备..."), a_filter, a_filterSize);
			IG::SameLine();
			IG::TextDisabled(I18n::T("%zu slot armour record(s) in this plugin; click a piece to add it.", "该 ESP 共有 %zu 条槽位护甲记录；点一件即可加入。"), armors.size());
			if (IG::BeginChild("##goldenpieces", ImVec2(0.0f, a_height), true, ImGuiWindowFlags_HorizontalScrollbar)) {
				const auto  needle = Lower(std::string(a_filter ? a_filter : ""));
				std::size_t shown = 0;
				std::size_t rowIndex = 0;
				for (const auto* armor : armors) {
					if (!armor) {
						continue;
					}
					const char* full = armor->GetFullName();
					const char* editor = armor->GetFormEditorID();
					const char* name = (full && full[0] != 0) ? full : ((editor && editor[0] != 0) ? editor : "");
					const auto  localId = PluginScan::LocalIdFor(a_plugin, armor->GetFormID());
					const auto  idText = HexLocalId(localId);
					if (!needle.empty() && !ContainsNoCase(name, needle) && !ContainsNoCase(idText, needle)) {
						continue;
					}
					++shown;
					const bool added = std::find(a_added.begin(), a_added.end(), localId) != a_added.end();
					char label[640]{};
					if (added) {
						std::snprintf(label, sizeof(label), I18n::T("[in the list] %s  %s", "[已加入] %s  %s"), name, idText.c_str());
					} else {
						std::snprintf(label, sizeof(label), "%s  %s", name, idText.c_str());
					}
					IG::PushID(static_cast<int>(rowIndex));
					++rowIndex;
					if (GoldenRow(label, added)) {
						std::string message;
						if (Golden::AddLocal(a_plugin, localId, message)) {
							g_goldenDirty = true;
							char text[512]{};
							std::snprintf(text, sizeof(text), I18n::T("Added %s.", "已添加 %s。"), message.c_str());
							g_goldenStatus = text;
							CreateRefreshAllArmorsTask();
						} else {
							g_goldenStatus = message;
						}
						RE::SendHUDMessage::ShowHUDMessage(g_goldenStatus.c_str(), nullptr, false);
						SKSE::log::info("Menu: golden piece {}:{} {}", a_plugin, idText, g_goldenStatus);
					}
					IG::SameLine();
					IG::TextDisabled(I18n::T("[slot %s]", "[槽位 %s]"), SlotNumbersFor(armor).c_str());
					IG::PopID();
				}
				if (shown == 0) {
					IG::TextDisabled(I18n::T("No piece of this plugin matches that search.", "该 ESP 里没有匹配的装备。"));
				}
			}
			IG::EndChild();
			IG::PopID();
		}

		void RenderGolden()
		{
			auto& settings = Settings::Get();
			const auto groups = Golden::Groups();
			IG::Text(I18n::T("Golden items: %zu piece(s) in %zu plugin group(s)", "金件：共 %zu 件，分布在 %zu 个 ESP 组"), Golden::TotalCount(), groups.size());
			IG::SameLine();
			IG::TextDisabled(I18n::T("A golden item is drawn on top of the outfit. Its file is separate from the outfit data.", "金件会盖在幻化之上显示；它的数据文件与幻化数据分开保存。"));
			IG::TextDisabled(I18n::T("Golden items take any record: FX effects, unnamed records and records without a model are allowed here.", "金件不挑记录：FX 特效、没有 EditorID、没有模型的记录都能加进来。"));
			IG::TextDisabled("Data\\SKSE\\Plugins\\OutfitSystemGoldenItems.json");
			if (g_goldenDirty) {
				IG::Text(I18n::T("[not saved] The changes live in memory only; press Export to write OutfitSystemGoldenItems.json.", "[未保存] 改动只在内存里生效，点「导出到 OutfitSystemGoldenItems.json」才会写盘保存。"));
			} else {
				IG::TextDisabled(I18n::T("Changes reach the file only when you press Export.", "只有点「导出」才会把改动写进文件。"));
			}

			// The selection can point at a group that was removed or replaced by an
			// import; drop it so the editor does not sit on a stale name.
			if (!g_goldenSelected.empty()) {
				const bool stillThere = std::any_of(groups.begin(), groups.end(), [](const Golden::Group& a_group) {
					return Lower(a_group.plugin) == Lower(g_goldenSelected);
				});
				if (!stillThere) {
					g_goldenSelected.clear();
				}
			}

			IG::SeparatorText(I18n::T("Editing", "编辑"));
			if (IG::Button(I18n::T("Add every piece of a plugin...", "添加某个 ESP 的全部装备..."))) {
				g_goldenPluginPicker = (g_goldenPluginPicker == 1) ? 0 : 1;
				g_goldenFilter[0] = 0;
			}
			IG::SameLine();
			if (IG::Button(I18n::T("Add one piece from a plugin...", "按 ESP 添加单件..."))) {
				g_goldenPluginPicker = (g_goldenPluginPicker == 2) ? 0 : 2;
				g_goldenFilter[0] = 0;
			}
			IG::SameLine();
			if (IG::Button(I18n::T("Export to OutfitSystemGoldenItems.json", "导出到 OutfitSystemGoldenItems.json"))) {
				const bool ok = Golden::SaveToDisk();
				if (ok) {
					g_goldenDirty = false;
					char text[512]{};
					std::snprintf(text, sizeof(text), I18n::T("Exported %zu piece(s) to %s", "已导出 %zu 件到 %s"), Golden::TotalCount(), Golden::JsonPath().c_str());
					g_goldenStatus = text;
				} else {
					g_goldenStatus = I18n::T("The golden item file could not be written; see the log.", "金件文件写不进去，详见日志。");
				}
				RE::SendHUDMessage::ShowHUDMessage(g_goldenStatus.c_str(), nullptr, false);
				SKSE::log::info("Menu: golden item export {}.", ok ? "succeeded" : "failed");
			}
			IG::SameLine();
			if (IG::Button(I18n::T("Import from OutfitSystemGoldenItems.json", "从 OutfitSystemGoldenItems.json 导入"))) {
				const bool ok = Golden::LoadFromDisk();
				if (ok) {
					g_goldenDirty = false;
					char text[512]{};
					std::snprintf(text, sizeof(text), I18n::T("Imported %zu piece(s) from %s", "已导入 %zu 件，来源 %s"), Golden::TotalCount(), Golden::JsonPath().c_str());
					g_goldenStatus = text;
				} else {
					g_goldenStatus = I18n::T("The golden item file could not be read; see the log.", "金件文件读不出来，详见日志。");
				}
				RE::SendHUDMessage::ShowHUDMessage(g_goldenStatus.c_str(), nullptr, false);
				SKSE::log::info("Menu: golden item import {}.", ok ? "succeeded" : "failed");
				if (ok) {
					CreateRefreshAllArmorsTask();
				}
			}

			if (g_goldenPluginPicker != 0) {
				IG::SetNextItemWidth(320.0f);
				IG::InputTextWithHint("##goldenpluginfilter", I18n::T("Search plugins...", "搜索 ESP..."), g_goldenFilter, sizeof(g_goldenFilter));
				IG::SameLine();
				IG::TextDisabled(g_goldenPluginPicker == 1 ? I18n::T("Add every slot armour of the chosen plugin.", "添加选中 ESP 的全部槽位护甲。") : I18n::T("Choose the plugin that holds the piece; its pieces are listed below.", "选择这件装备所属的 ESP，它的全部装备会列在下面。"));
				if (IG::BeginChild("##goldenplugins", ImVec2(0.0f, 200.0f), true, ImGuiWindowFlags_HorizontalScrollbar)) {
					const auto  plugins = PluginScan::PluginsWithArmor();
					const auto  needle = Lower(g_goldenFilter);
					std::size_t shown = 0;
					for (auto it = plugins.rbegin(); it != plugins.rend(); ++it) {
						if (!ContainsNoCase(it->name, needle)) {
							continue;
						}
						++shown;
						char label[320]{};
						std::snprintf(label, sizeof(label), I18n::T("%s  (%zu piece(s))", "%s（%zu 件）"), it->name.c_str(), it->armors);
						IG::PushID(static_cast<int>(it->order));
						if (GoldenRow(label, false)) {
							if (g_goldenPluginPicker == 1) {
								const auto added = Golden::AddAllFromPlugin(it->name);
								g_goldenDirty = true;
								g_goldenSelected = it->name;
								{
					char text[512]{};
					std::snprintf(text, sizeof(text), I18n::T("%zu piece(s) of %s added.", "已添加 %zu 件，来自 %s。"), added, it->name.c_str());
					g_goldenStatus = text;
				}
								RE::SendHUDMessage::ShowHUDMessage(g_goldenStatus.c_str(), nullptr, false);
								CreateRefreshAllArmorsTask();
							} else {
								// 1.5.45: picking the plugin opens its own piece list, so the
								// piece is clicked out of that plugin instead of typed in.
								g_goldenSelected = it->name;
								g_goldenPiecePlugin = it->name;
								g_goldenPieceFilter[0] = 0;
								g_goldenStatus = I18n::T("Pick a piece of that plugin below.", "在下面点一件该 ESP 的装备。");
							}
							g_goldenPluginPicker = 0;
						}
						IG::PopID();
					}
					if (shown == 0) {
						IG::TextDisabled(I18n::T("No plugin matches that search.", "没有匹配的 ESP。"));
					}
				}
				IG::EndChild();
			}

			if (!g_goldenPiecePlugin.empty()) {
				IG::SeparatorText(g_goldenPiecePlugin.c_str());
				RenderGoldenPieceList(g_goldenPiecePlugin, AddedIdsOf(groups, g_goldenPiecePlugin), 1, 220.0f, g_goldenPieceFilter, sizeof(g_goldenPieceFilter));
				if (IG::Button(I18n::T("Close the piece list", "关闭装备列表"))) {
					g_goldenPiecePlugin.clear();
				}
			}

			// Keep both panes inside the window: a list wider than the free space
			// used to push the editor pane over the list, and the pane drawn last
			// swallowed the clicks meant for the rows underneath it.
			const float avail = IG::ContentRegionAvail().x;
			const float listWidth = std::min(static_cast<float>(std::clamp(settings.outfitListWidth, 240, 1200)), std::max(240.0f, avail - 320.0f));
			if (IG::BeginChild("##goldengroups", ImVec2(listWidth, 440.0f), true, ImGuiWindowFlags_HorizontalScrollbar)) {
				if (groups.empty()) {
					IG::TextDisabled(I18n::T("No golden items yet - add a plugin above, or pick a single piece out of a plugin.", "还没有金件 - 先在上面添加一个 ESP，或从某个 ESP 里挑一件。"));
				}
				std::size_t rowIndex = 0;
				for (const auto& group : groups) {
					// The id must be the same on the frame the button goes down and the
					// frame it comes up: Golden::Groups() hands out a fresh copy every
					// frame, so PushID(plugin.c_str()) pushed a different pointer each
					// frame and ImGui dropped the click (the row looked unclickable).
					IG::PushID(static_cast<int>(rowIndex));
					char label[384]{};
					std::snprintf(label, sizeof(label), "%s  (%zu)", group.plugin.c_str(), group.localIds.size());
					const bool isSelected = !g_goldenSelected.empty() && Lower(group.plugin) == Lower(g_goldenSelected);
					if (GoldenRow(label, isSelected)) {
						g_goldenSelected = group.plugin;
						g_goldenEditorFilter[0] = 0;
						SKSE::log::info("Menu: golden group '{}' selected ({} of {}).", group.plugin, rowIndex + 1, groups.size());
					}
					IG::PopID();
					++rowIndex;
				}
			}
			IG::EndChild();

			IG::SameLine();
			const float editorWidth = std::max(320.0f, IG::ContentRegionAvail().x);
			if (IG::BeginChild("##goldeneditor", ImVec2(editorWidth, 440.0f), true, ImGuiWindowFlags_HorizontalScrollbar)) {
				const Golden::Group* selected = nullptr;
				for (const auto& group : groups) {
					if (Lower(group.plugin) == Lower(g_goldenSelected)) {
						selected = &group;
						break;
					}
				}
				if (!selected) {
					IG::TextDisabled(I18n::T("Select a plugin group on the left to edit its pieces.", "在左侧选中一个 ESP 组来编辑它的金件。"));
				} else {
					IG::SeparatorText(selected->plugin.c_str());
					IG::Text(I18n::T("%zu golden piece(s); the plugin holds %zu slot armour record(s).", "金件 %zu 件；该 ESP 共有 %zu 条槽位护甲记录。"), selected->localIds.size(), PluginScan::ArmorsOf(selected->plugin).size());
					if (IG::Button(I18n::T("Remove every piece of this plugin", "移除该 ESP 的全部金件"))) {
						if (Golden::RemoveGroup(selected->plugin)) {
							g_goldenDirty = true;
							{
							char text[512]{};
							std::snprintf(text, sizeof(text), I18n::T("Removed the %s group.", "已移除 %s 组。"), selected->plugin.c_str());
							g_goldenStatus = text;
						}
							g_goldenSelected.clear();
							CreateRefreshAllArmorsTask();
						}
					}
					IG::Separator();
					for (const auto localId : selected->localIds) {
						// Any armour record counts here as well; a golden item may be
						// a piece the outfit generator would skip (FX, no model).
						const auto* form = PluginScan::ResolveForm(selected->plugin, localId);
						const auto* armor = form ? form->As<RE::TESObjectARMO>() : nullptr;
						const char* name = (armor && armor->GetFullName()) ? armor->GetFullName() : I18n::T("<not in this load order>", "<当前加载顺序里没有>");
						IG::PushID(static_cast<int>(localId));
						// The remove button comes first: with a long piece name the
						// buttons used to sit past the right edge of the pane.
						if (IG::SmallButton(I18n::T("Remove", "移除"))) {
							if (Golden::RemoveLocal(selected->plugin, localId)) {
								g_goldenDirty = true;
								{
								char text[512]{};
								std::snprintf(text, sizeof(text), I18n::T("Removed %s:%s.", "已移除 %s:%s。"), selected->plugin.c_str(), HexLocalId(localId).c_str());
								g_goldenStatus = text;
							}
								CreateRefreshAllArmorsTask();
							}
						}
						IG::SameLine();
						char line[512]{};
						std::snprintf(line, sizeof(line), "%s:%s  %s", selected->plugin.c_str(), HexLocalId(localId).c_str(), name);
						IG::Text("%s", line);
						IG::SameLine();
						IG::TextDisabled(I18n::T("[slot %s]", "[槽位 %s]"), SlotNumbersFor(armor).c_str());
						IG::PopID();
					}
					IG::Separator();
					IG::TextDisabled(I18n::T("Pick a piece of this plugin below; the list holds only pieces of the selected plugin.", "在下面点一件该 ESP 的装备；列表只含选中 ESP 的装备。"));
					RenderGoldenPieceList(selected->plugin, selected->localIds, 2, 200.0f, g_goldenEditorFilter, sizeof(g_goldenEditorFilter));
				}
			}
			IG::EndChild();

			if (!g_goldenStatus.empty()) {
				IG::Separator();
				IG::TextWrapped("%s", g_goldenStatus.c_str());
			}
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
		void RenderGoldenThunk() { UpdateTextInputState(); FlushSettingsIfDue(); RenderGolden(); }
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
		// Menu lifecycle: a hotkey recording is ended from here when the player
		// closes the menu while the recorder window is still open.
		MenuBridge::RegisterEventCallback(OnFrameworkMenuEvent);
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
		const auto goldenPath = sectionPath("Golden items");
		const auto settingsPath = sectionPath("Settings");
		MenuBridge::AddSectionItem(quickPath.c_str(), RenderQuickPickerSectionThunk);
		MenuBridge::AddSectionItem(outfitPath.c_str(), RenderOutfitsThunk);
		MenuBridge::AddSectionItem(actorsPath.c_str(), RenderActorsThunk);
		MenuBridge::AddSectionItem(goldenPath.c_str(), RenderGoldenThunk);
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
