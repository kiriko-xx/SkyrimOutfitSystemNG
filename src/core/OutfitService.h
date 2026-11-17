#pragma once

#include "core/Outfit.h"
#include "core/Types.h"

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace SKSE
{
	class SerializationInterface;
}

namespace SOS
{
	struct ActorAssignments
	{
		std::optional<std::string> current;                                   // canonical (lower-case) outfit key
		bool stateBasedAutoSwitchingEnabled{ false };
		std::array<std::optional<std::string>, kStateTypeCount> stateBased{};  // indexed by StateType
		// Runtime only: the state whose outfit was applied last. Used to keep a
		// random pick stable while the actor stays in the same state (combat
		// events fire several times per fight). Not serialised.
		std::optional<std::uint32_t> lastState;
	};

	class OutfitServiceReadGuard;
	class OutfitServiceWriteGuard;

	class OutfitService
	{
	public:
		static OutfitService& Get();

		// ---- outfits -------------------------------------------------------
		[[nodiscard]] std::int32_t max_outfit_name_len() const { return kMaxNameLength; }
		[[nodiscard]] const Outfit* get_outfit_ptr(const char* a_name) const;
		// Returns a writable outfit. The store revision is bumped on the way out:
		// every caller mutates through the returned pointer (the Outfit itself
		// knows nothing about the service), and the revision is the only signal the
		// skinning hooks and the menu caches have. Skipping it leaves the illusion
		// showing the previous contents.
		[[nodiscard]] Outfit* get_mut_outfit_ptr(const char* a_name);
		// Bumps the revision after a write that did not go through one of the
		// helpers above (the clipboard writes whole outfits through
		// get_or_create_mut_outfit_ptr).
		void notify_outfit_changed() { ++_revision; }
		// Every actor whose currently applied outfit is \a a_name. Used to refresh
		// exactly the actors a change to that outfit can be seen on.
		[[nodiscard]] std::vector<RE::FormID> actors_wearing(const char* a_name) const;
		[[nodiscard]] Outfit* get_or_create_mut_outfit_ptr(const char* a_name);
		bool add_outfit(const char* a_name);
		bool delete_outfit(const char* a_name);
		[[nodiscard]] bool has_outfit(const char* a_name) const;
		void set_favorite(const char* a_name, bool a_favorite);
		bool modify_outfit(const char* a_name, const std::vector<const RE::TESObjectARMO*>& a_add, const std::vector<const RE::TESObjectARMO*>& a_remove, bool a_createIfNeeded);
		// Replaces every piece of an outfit in one step (Papyrus OverwriteOutfit).
		bool replace_outfit_armors(const char* a_name, const std::vector<const RE::TESObjectARMO*>& a_armors, bool a_createIfNeeded);
		// Creates an outfit and fills in every part of it (armour pieces, blanket
		// policy, per slot overrides) in one locked step. Used by the clipboard
		// paste in the menu and whenever a whole preset arrives from outside; the
		// slot policies are reset first so an absent value cannot keep a stale
		// default. Returns false when the name is empty, too long or already used.
		bool create_outfit_preset(const std::string& a_name, const std::vector<const RE::TESObjectARMO*>& a_armors, const std::string& a_blanketPolicy, const std::vector<std::pair<std::uint32_t, std::string>>& a_slotPolicies, bool a_favorite);
		std::uint32_t rename_outfit(const char* a_name, const char* a_newName);
		[[nodiscard]] rust::Vec<rust::String> get_outfit_names(bool a_favoritesOnly) const;
		[[nodiscard]] std::vector<rust::String> all_outfit_names() const;
		// Monotonic counter bumped whenever the outfit list or a visible
		// property (name/favourite) changes. The UI uses it to avoid copying
		// every outfit name on every frame and to notice renames that keep the
		// total count unchanged.
		[[nodiscard]] std::uint64_t revision() const { return _revision.load(std::memory_order_relaxed); }
		[[nodiscard]] std::size_t outfit_count() const
		{
			std::lock_guard lock(_mutex);
			return _outfits.size();
		}

		// ---- actors --------------------------------------------------------
		[[nodiscard]] const Outfit* current_outfit_ptr(RE::FormID a_actor) const;
		[[nodiscard]] Outfit* current_outfit_mut_ptr(RE::FormID a_actor);
		void set_outfit_c(const char* a_name, RE::FormID a_actor);
		void add_actor(RE::FormID a_actor);
		void remove_actor(RE::FormID a_actor);
		[[nodiscard]] rust::Vec<std::uint32_t> list_actors() const;
		[[nodiscard]] bool should_override(RE::FormID a_actor) const;

		// ---- state based switching ----------------------------------------
		// The sentinel the UI stores when a state should pick a random outfit
		// every time it is entered. It is not a real outfit and is never
		// written to the outfit list; it lives in a state slot instead.
		static constexpr const char* kRandomStateOutfit = "<random>";
		void set_state_based_switching_enabled(bool a_enabled, RE::FormID a_actor);
		[[nodiscard]] bool get_state_based_switching_enabled_c(RE::FormID a_actor) const;
		// True when at least one of the 13 state slots is configured. The state
		// switching must work even without a base outfit, otherwise an actor that
		// only uses state outfits never switches at all.
		[[nodiscard]] bool has_any_state_outfit(RE::FormID a_actor) const;
		void set_outfit_using_state(StateType a_state, RE::FormID a_actor);
		void set_state_outfit(StateType a_state, RE::FormID a_actor, const char* a_name);
		void unset_state_outfit(StateType a_state, RE::FormID a_actor);
		[[nodiscard]] rust::String get_state_outfit_name_c(StateType a_state, RE::FormID a_actor) const;
		[[nodiscard]] OptionalStateType check_location_type_c(
			const rust::Vec<rust::String>& a_keywords,
			WeatherFlags a_weather,
			bool a_isInCombat,
			RE::FormID a_actor) const;

		// ---- global --------------------------------------------------------
		void set_enabled(bool a_enabled) { _enabled.store(a_enabled, std::memory_order_relaxed); }
		[[nodiscard]] bool enabled_c() const { return _enabled.load(std::memory_order_relaxed); }

		// ---- persistence ---------------------------------------------------
		[[nodiscard]] rust::String save_json_c() const;
		bool replace_with_json_data(rust::Str a_json, const SKSE::SerializationInterface* a_intfc);

		// The on-disk format of the original mod
		// (Data/SKSE/Plugins/OutfitSystemData.json, written with the protobuf
		// JSON mapping). Kept compatible in both directions so a file exported
		// by the old version imports here and a file written here can be read
		// by the old version. Field names are lowerCamelCase, defaults are
		// omitted and map keys are strings, exactly like protobuf does.
		[[nodiscard]] rust::String save_legacy_json_c() const;
		bool replace_with_legacy_json(rust::Str a_json, const SKSE::SerializationInterface* a_intfc);
		// Reads a co-save record written by the original (protobuf based) mod,
		// versions 4..6, and converts it into the in-memory store.
		bool replace_with_legacy_proto(const void* a_data, std::size_t a_length, std::uint32_t a_version, const SKSE::SerializationInterface* a_intfc);
		void replace_with_new();
		void check_consistency();

		static constexpr std::int32_t kMaxNameLength = 256;

	private:
		friend class OutfitServiceReadGuard;
		friend class OutfitServiceWriteGuard;
		OutfitService() = default;

		[[nodiscard]] static std::string Canonical(std::string_view a_name);
		[[nodiscard]] Outfit* FindMutable(const std::string& a_key);
		[[nodiscard]] const Outfit* Find(const std::string& a_key) const;
		[[nodiscard]] std::string PickRandomStateOutfit(const std::string& a_exclude) const;
		[[nodiscard]] ActorAssignments* FindAssignment(RE::FormID a_actor);
		[[nodiscard]] const ActorAssignments* FindAssignment(RE::FormID a_actor) const;
		[[nodiscard]] static bool IsFormIDPermitted(RE::FormID a_form);

		mutable std::recursive_mutex _mutex;
		std::atomic<std::uint64_t> _revision{ 0 };
		std::atomic<bool> _enabled{ true };
		std::unordered_map<std::string, Outfit> _outfits;            // key: canonical name
		std::unordered_map<RE::FormID, ActorAssignments> _assignments;
	};

	// RAII guards that mirror the read/write mutex objects of the former cxx bridge.
	//
	// The destructor MUST unlock: the former Rust bridge released the mutex when
	// the guard was dropped. A guard that never unlocks leaves the recursive
	// mutex owned by the thread that created it, so the first call from another
	// thread (the render thread runs the skinning hooks!) blocks forever and the
	// game hangs - which is exactly the "load screen never finishes" bug.
	class OutfitServiceReadGuard
	{
	public:
		OutfitServiceReadGuard();
		~OutfitServiceReadGuard();
		OutfitServiceReadGuard(const OutfitServiceReadGuard&) = delete;
		OutfitServiceReadGuard& operator=(const OutfitServiceReadGuard&) = delete;
		OutfitService& inner() { return OutfitService::Get(); }
		// Lets the stack guard keep the historical "service->inner()" spelling.
		OutfitServiceReadGuard* operator->() { return this; }
	};

	class OutfitServiceWriteGuard
	{
	public:
		OutfitServiceWriteGuard();
		~OutfitServiceWriteGuard();
		OutfitServiceWriteGuard(const OutfitServiceWriteGuard&) = delete;
		OutfitServiceWriteGuard& operator=(const OutfitServiceWriteGuard&) = delete;
		OutfitService& inner() { return OutfitService::Get(); }
		// Lets the stack guard keep the historical "service->inner()" spelling.
		OutfitServiceWriteGuard* operator->() { return this; }
	};

	// Returned by value: the guard locks in its constructor and unlocks in its
	// destructor, so a stack temporary is enough. The old unique_ptr return type
	// allocated on every call, including the per-frame skinning hooks.
	OutfitServiceReadGuard outfit_service_get_singleton_ptr();
	OutfitServiceWriteGuard outfit_service_get_mut_singleton_ptr();
}// namespace SOS

using SOS::outfit_service_get_mut_singleton_ptr;
using SOS::outfit_service_get_singleton_ptr;
using SOS::Outfit;
using SOS::OutfitService;
