#pragma once

#include "core/GoldenItems.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace RE
{
	class TESObjectARMO;
}

namespace SOS
{
	class Settings
	{
	public:
		static Settings& Get();

		void Load();

		// Writes the current values back to the ini (menu edits, .ini overrides).
		void Save();

		// Returns true if the given armor must always win over an outfit item
		// (used for "golden" items such as the Dragon Aspect armor).
		bool IsGoldenItem(const RE::TESObjectARMO* a_armor) const;

		// Called after a game load so that resolved form ids are refreshed.
		void InvalidateResolvedForms();

		// Resolve the configured "golden" forms once, on the main thread, before
		// the skinning hooks can call IsGoldenItem() from the render thread. A
		// lazy first call from the render thread would race with the menu thread.
		void WarmUp() const
		{
			if (!_goldenResolved.load(std::memory_order_acquire)) {
				std::lock_guard lock(_goldenMutex);
				if (!_goldenResolved.load(std::memory_order_relaxed)) {
					ResolveGoldenItems();
				}
			}
			// The golden item set itself lives in its own store (SOS::Golden) so the
			// menu can add and remove pieces; resolving it here keeps the render
			// thread away from the first, slow lookup.
			Golden::WarmUp();
		}

		bool enabled{ true };

		// [Debug] - lets the user disable parts of the plugin to isolate a crash.
		bool installHooks{ true };

		// [Compatibility] - targeted integrations with mods that ask the game what
		// the player is wearing. The generic Papyrus "perception" hooks were
		// removed: no installed mod called them.
		bool compatAdvancedNudityDetection{ true };
		bool compatProceduralArmorDamage{ true };

		// [Compatibility] - reset the Faster HDT-SMP physics of the actor whose
		// outfit changed. Only that actor is touched.
		bool autoResetSmp{ false };

		// [Compatibility] - during a SexLab / OStim scene wear "<current outfit>_sex"
		// when an outfit with that name exists.
		bool sceneOutfitSwitching{ false };

		// [Compatibility] - milliseconds between the two passes of the .tri
		// refresh cycle (illusion off, then on). Zero keeps both passes in the
		// same frame, which hides the intermediate real-armour state.
		int reillusionGapMs{ 0 };

		// [Compatibility] - when a body morph provider answered (RaceMenu's
		// BodyMorph interface or OBody), ask it for one ApplyBodyMorphs pass
		// instead of taking the illusion off for an engine pass over the real
		// gear. The provider pass costs one body rebuild instead of two full worn
		// rebuilds and never shows the real armour; the cycle stays as the
		// verified fallback for the cases the provider cannot reach.
		bool morphProviderFirst{ true };

		// [RandomOutfit]
		// One binding per device: the keyboard chord and the controller chord are
		// separate values, so recording one never replaces the other and both stay
		// usable. [RandomOutfit] Hotkey / HotkeyGamepad, QuickPickerHotkey /
		// QuickPickerHotkeyGamepad.
		std::string randomOutfitHotkey{ "ctrl+shift+O" };
		std::string randomOutfitGamepadHotkey{};
		std::string quickPickerHotkey{ "ctrl+shift+L" };
		std::string quickPickerGamepadHotkey{};

		// [RandomOutfit] - a single key without modifiers that opens the quick
		// picker the moment it is pressed. The "Change Outfit" lesser power that
		// the shipped esp grants only reaches the window after its cast animation,
		// which is what feels like a cooldown on the quickslot key; this key skips
		// that wait and is limited to one open every 0.5 s. The power keeps
		// working (its own open call does nothing while the window is already
		// open), and the key is ignored while QuickSlotRandom is on so the
		// random roll never happens twice. Empty disables the shortcut.
		std::string quickPickerKey{ "Z" };
		bool randomFavoritesOnly{ true };
		bool randomExcludeCurrent{ true };

		// [RandomOutfit] - true: the "Change Outfit" power rolls a random outfit
		// instead of opening the quick picker window.
		bool quickslotRandom{ false };

		// [General] - adds the "Change Outfit" lesser power to the player, the
		// switch that the options page of the old MCM exposed.
		bool quickslotsEnabled{ true };
		bool extraLogging{ false };

		// [UI] - "auto" follows the game language, "en" forces English, "zh"
		// forces Chinese (falls back to English when no CJK font is loaded).
		std::string language{ "auto" };

		// [UI] - list layout, shared by the outfits browser and the quick picker
		// so the choice survives restarts instead of resetting to the default.
		int outfitPageSize{ 50 };
		int quickPickerPageSize{ 20 };
		int outfitListWidth{ 470 };

		// [UI] - geometry of the quick picker window in pixels. -1 means "the
		// user never moved it", in which case the window opens centred at its
		// default size. Written back whenever the window is moved or resized so
		// the position survives a restart (ImGui itself has no ini file inside
		// the game process).
		int quickPickerX{ -1 };
		int quickPickerY{ -1 };
		int quickPickerWidth{ 640 };
		int quickPickerHeight{ 800 };

		bool setDefaultSlotPolicies{ true };
		std::string defaultBasePolicy{ "XXOO" };

		// [Customization] - outfits generated from a plugin are marked as
		// favourites so they are easy to find in the list afterwards.
		bool espOutfitAutoFavorite{ true };

		// plugin name -> local form id. This is the legacy list of the ini: it is
		// moved into the golden item store once (see goldenIniMigrated) and is empty
		// afterwards. Kept so an existing ini keeps working.
		std::vector<std::pair<std::string, RE::FormID>> goldenItems;
		bool goldenIniMigrated{ false };

	private:
		void ResolveGoldenItems() const;

		mutable std::atomic<bool> _goldenResolved{ false };
		mutable std::mutex _goldenMutex;
		// Read from the render thread (skinning hooks) and rebuilt from the main
		// thread, so the set is published as an immutable snapshot instead of
		// being mutated in place. std::atomic<std::shared_ptr<T>> is C++20 and
		// keeps the fast path lock free.
		mutable std::shared_ptr<const std::unordered_set<RE::FormID>> _resolvedGolden;
		// Same reasoning for the configured list itself: the menu can reload the
		// ini while the render thread resolves the items.
		mutable std::shared_ptr<const std::vector<std::pair<std::string, RE::FormID>>> _goldenSnapshot;
	};
}// namespace SOS
