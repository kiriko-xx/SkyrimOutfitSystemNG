#include "core/OutfitService.h"

#include "core/Policy.h"
#include "core/Settings.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <random>

namespace SOS
{
	namespace
	{
		const char* const kLocationCityKeyword = "LocTypeCity";
		const char* const kLocationTownKeyword = "LocTypeTown";
		const char* const kLocationDungeonKeyword = "LocTypeDungeon";

		bool HasKeyword(const rust::Vec<rust::String>& a_keywords, const char* a_keyword)
		{
			for (const auto& keyword : a_keywords) {
				if (keyword == a_keyword) {
					return true;
				}
			}
			return false;
		}

		RE::TESObjectARMO* ResolveArmor(const nlohmann::json& a_entry, const SKSE::SerializationInterface* a_intfc)
		{
			if (!a_entry.is_object()) {
				return nullptr;
			}
			RE::FormID raw = a_entry.value("form", 0u);
			if (raw == 0) {
				return nullptr;
			}
			RE::FormID resolved = raw;
			if (a_intfc) {
				a_intfc->ResolveFormID(raw, resolved);
			}
			return RE::TESForm::LookupByID<RE::TESObjectARMO>(resolved);
		}
	}

	OutfitService& OutfitService::Get()
	{
		static OutfitService s_service;
		return s_service;
	}

	std::string OutfitService::Canonical(std::string_view a_name)
	{
		std::string result(a_name);
		std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});
		return result;
	}

	const Outfit* OutfitService::Find(const std::string& a_key) const
	{
		auto it = _outfits.find(a_key);
		return it == _outfits.end() ? nullptr : std::addressof(it->second);
	}

	Outfit* OutfitService::FindMutable(const std::string& a_key)
	{
		auto it = _outfits.find(a_key);
		return it == _outfits.end() ? nullptr : std::addressof(it->second);
	}

	const Outfit* OutfitService::get_outfit_ptr(const char* a_name) const
	{
		if (!a_name) {
			return nullptr;
		}
		return Find(Canonical(a_name));
	}

	Outfit* OutfitService::get_mut_outfit_ptr(const char* a_name)
	{
		if (!a_name) {
			return nullptr;
		}
		auto* outfit = FindMutable(Canonical(a_name));
		if (outfit) {
			// See the header: the caller edits through this pointer, so this is the
			// only place that can tell the hooks and the menus about it.
			++_revision;
		}
		return outfit;
	}

	std::vector<RE::FormID> OutfitService::actors_wearing(const char* a_name) const
	{
		std::vector<RE::FormID> result;
		if (!a_name || !*a_name) {
			return result;
		}
		const auto key = Canonical(a_name);
		std::lock_guard lock(_mutex);
		result.reserve(_assignments.size());
		for (const auto& [actor, assignment] : _assignments) {
			if (assignment.current && Canonical(*assignment.current) == key) {
				result.push_back(actor);
			}
		}
		return result;
	}

	Outfit* OutfitService::get_or_create_mut_outfit_ptr(const char* a_name)
	{
		if (!a_name || !*a_name || std::string_view(a_name).size() > kMaxNameLength) {
			return nullptr;
		}
		auto key = Canonical(a_name);
		if (auto* existing = FindMutable(key)) {
			return existing;
		}
		auto [it, inserted] = _outfits.emplace(key, Outfit(std::string(a_name)));
		if (inserted) {
			++_revision;
		}
		return std::addressof(it->second);
	}

	bool OutfitService::add_outfit(const char* a_name)
	{
		if (!a_name || !*a_name || std::string_view(a_name).size() > kMaxNameLength) {
			return false;
		}
		auto key = Canonical(a_name);
		if (_outfits.contains(key)) {
			return false;
		}
		_outfits.emplace(key, Outfit(std::string(a_name)));
		++_revision;
		return true;
	}

	bool OutfitService::create_outfit_preset(const std::string& a_name, const std::vector<const RE::TESObjectARMO*>& a_armors, const std::string& a_blanketPolicy, const std::vector<std::pair<std::uint32_t, std::string>>& a_slotPolicies, bool a_favorite)
	{
		if (a_name.empty() || a_name.size() > kMaxNameLength) {
			return false;
		}
		auto key = Canonical(a_name);
		if (_outfits.contains(key)) {
			return false;
		}
		auto [it, inserted] = _outfits.emplace(key, Outfit(a_name));
		if (!inserted) {
			return false;
		}
		auto& outfit = it->second;
		outfit.set_favorite(a_favorite);
		outfit.erase_all_armors();
		for (const auto* armor : a_armors) {
			if (armor) {
				outfit.insert_armor(armor);
			}
		}
		// Start from "nothing is shown" and then apply what the preset actually
		// carries, exactly like the legacy save import does. Without the reset a
		// missing policy would silently inherit the constructor default (XXOO
		// plus the shield override) which is not what the pasted outfit said.
		for (std::uint32_t slot = 0; slot < kSlotCount; ++slot) {
			outfit.set_slot_policy_raw(slot, std::nullopt);
		}
		outfit.set_blanket_policy_raw(a_blanketPolicy.empty() ? std::string("XXXX") : a_blanketPolicy);
		for (const auto& [slot, code] : a_slotPolicies) {
			if (slot < kSlotCount && !code.empty()) {
				outfit.set_slot_policy_raw(slot, code);
			}
		}
		++_revision;
		return true;
	}

	bool OutfitService::delete_outfit(const char* a_name)
	{
		if (!a_name) {
			return false;
		}
		auto key = Canonical(a_name);
		if (_outfits.erase(key) == 0) {
			return false;
		}
		++_revision;
		for (auto& [formId, assignment] : _assignments) {
			if (assignment.current == key) {
				assignment.current.reset();
			}
			for (auto& state : assignment.stateBased) {
				if (state == key) {
					state.reset();
				}
			}
		}
		return true;
	}

	bool OutfitService::has_outfit(const char* a_name) const
	{
		if (!a_name) {
			return false;
		}
		return _outfits.contains(Canonical(a_name));
	}

	void OutfitService::set_favorite(const char* a_name, bool a_favorite)
	{
		if (auto* outfit = get_mut_outfit_ptr(a_name)) {
			if (outfit->favorite_c() != a_favorite) {
				outfit->set_favorite(a_favorite);
				++_revision;
			}
		}
	}

	bool OutfitService::modify_outfit(const char* a_name, const std::vector<const RE::TESObjectARMO*>& a_add, const std::vector<const RE::TESObjectARMO*>& a_remove, bool a_createIfNeeded)
	{
		if (!a_name) {
			return false;
		}
		auto* outfit = a_createIfNeeded ? get_or_create_mut_outfit_ptr(a_name) : get_mut_outfit_ptr(a_name);
		if (!outfit) {
			return false;
		}
		for (const auto* armor : a_remove) {
			outfit->erase_armor(armor);
		}
		for (const auto* armor : a_add) {
			outfit->insert_armor(armor);
		}
		++_revision;
		return true;
	}

	bool OutfitService::replace_outfit_armors(const char* a_name, const std::vector<const RE::TESObjectARMO*>& a_armors, bool a_createIfNeeded)
	{
		if (!a_name) {
			return false;
		}
		auto* outfit = a_createIfNeeded ? get_or_create_mut_outfit_ptr(a_name) : get_mut_outfit_ptr(a_name);
		if (!outfit) {
			return false;
		}
		outfit->erase_all_armors();
		for (const auto* armor : a_armors) {
			if (armor) {
				outfit->insert_armor(armor);
			}
		}
		++_revision;
		return true;
	}

	std::uint32_t OutfitService::rename_outfit(const char* a_name, const char* a_newName)
	{
		if (!a_name || !a_newName || !*a_newName || std::string_view(a_newName).size() > kMaxNameLength) {
			return 1;
		}
		auto from = Canonical(a_name);
		auto to = Canonical(a_newName);
		if (from == to) {
			return 0;
		}
		auto it = _outfits.find(from);
		if (it == _outfits.end()) {
			return 1;
		}
		if (_outfits.contains(to)) {
			return 2;
		}
		Outfit moved = std::move(it->second);
		_outfits.erase(it);
		moved.set_name(std::string(a_newName));
		_outfits.emplace(to, std::move(moved));

		++_revision;
		for (auto& [formId, assignment] : _assignments) {
			if (assignment.current == from) {
				assignment.current = to;
			}
			for (auto& state : assignment.stateBased) {
				if (state == from) {
					state = to;
				}
			}
		}
		return 0;
	}

	std::vector<rust::String> OutfitService::all_outfit_names() const
	{
		std::lock_guard lock(_mutex);
		std::vector<rust::String> result;
		result.reserve(_outfits.size());
		for (const auto& [key, outfit] : _outfits) {
			result.emplace_back(outfit.name_c());
		}
		// unordered_map iteration order is arbitrary; every caller (menus,
		// combos, the quick picker) expects a stable alphabetical list.
		std::sort(result.begin(), result.end(), [](const rust::String& a_lhs, const rust::String& a_rhs) {
			return _stricmp(a_lhs.c_str(), a_rhs.c_str()) < 0;
		});
		return result;
	}

	rust::Vec<rust::String> OutfitService::get_outfit_names(bool a_favoritesOnly) const
	{
		std::lock_guard lock(_mutex);
		rust::Vec<rust::String> result;
		result.reserve(_outfits.size());
		for (const auto& [key, outfit] : _outfits) {
			if (a_favoritesOnly && !outfit.favorite_c()) {
				continue;
			}
			result.emplace_back(outfit.name_c());
		}
		std::sort(result.begin(), result.end(), [](const rust::String& a_lhs, const rust::String& a_rhs) {
			return _stricmp(a_lhs.c_str(), a_rhs.c_str()) < 0;
		});
		return result;
	}

	const ActorAssignments* OutfitService::FindAssignment(RE::FormID a_actor) const
	{
		auto it = _assignments.find(a_actor);
		return it == _assignments.end() ? nullptr : std::addressof(it->second);
	}

	ActorAssignments* OutfitService::FindAssignment(RE::FormID a_actor)
	{
		auto it = _assignments.find(a_actor);
		return it == _assignments.end() ? nullptr : std::addressof(it->second);
	}

	const Outfit* OutfitService::current_outfit_ptr(RE::FormID a_actor) const
	{
		const auto* assignment = FindAssignment(a_actor);
		if (!assignment || !assignment->current) {
			return nullptr;
		}
		return Find(*assignment->current);
	}

	Outfit* OutfitService::current_outfit_mut_ptr(RE::FormID a_actor)
	{
		const auto* assignment = FindAssignment(a_actor);
		if (!assignment || !assignment->current) {
			return nullptr;
		}
		return FindMutable(*assignment->current);
	}

	void OutfitService::set_outfit_c(const char* a_name, RE::FormID a_actor)
	{
		if (!a_name || !*a_name) {
			// An empty name clears the assignment (the quickslot menu uses this for
			// its "[NO OUTFIT]" entry).
			_assignments[a_actor].current.reset();
			// The skinning hook watches the revision, so a cleared illusion must
			// bump it as well; otherwise Z / "no outfit" changes stay invisible.
			++_revision;
			return;
		}
		auto key = Canonical(a_name);
		if (!_outfits.contains(key)) {
			return;
		}
		_assignments[a_actor].current = key;
		// Re-applying the same outfit is a user action that must rebuild the
		// illusion (a stale .tri clone otherwise survives, see 1.5.13), so this
		// bumps the revision even when the value did not change.
		++_revision;
	}

	void OutfitService::add_actor(RE::FormID a_actor)
	{
		if (!IsFormIDPermitted(a_actor)) {
			return;
		}
		if (_assignments.try_emplace(a_actor).second) {
			// A new assignment makes the actor a part of the illusion set; the
			// skinning hook keys its rebuild off the revision, so it has to move.
			++_revision;
		}
	}

	void OutfitService::remove_actor(RE::FormID a_actor)
	{
		const auto* player = RE::PlayerCharacter::GetSingleton();
		if (player && a_actor == player->GetFormID()) {
			SKSE::log::warn("Refusing to unassign the player character.");
			return;
		}
		if (_assignments.erase(a_actor) != 0) {
			// The illusion of a removed actor has to be taken off, which only
			// happens when the revision moves (see 1.5.13).
			++_revision;
		}
	}

	rust::Vec<std::uint32_t> OutfitService::list_actors() const
	{
		rust::Vec<std::uint32_t> result;
		result.reserve(_assignments.size());
		for (const auto& [formId, assignment] : _assignments) {
			result.push_back(static_cast<std::uint32_t>(formId));
		}
		return result;
	}

	bool OutfitService::should_override(RE::FormID a_actor) const
	{
		return _enabled && current_outfit_ptr(a_actor) != nullptr;
	}

	void OutfitService::set_state_based_switching_enabled(bool a_enabled, RE::FormID a_actor)
	{
		_assignments[a_actor].stateBasedAutoSwitchingEnabled = a_enabled;
		++_revision;
	}

	bool OutfitService::get_state_based_switching_enabled_c(RE::FormID a_actor) const
	{
		const auto* assignment = FindAssignment(a_actor);
		return assignment ? assignment->stateBasedAutoSwitchingEnabled : false;
	}

	bool OutfitService::has_any_state_outfit(RE::FormID a_actor) const
	{
		const auto* assignment = FindAssignment(a_actor);
		if (!assignment) {
			return false;
		}
		for (const auto& state : assignment->stateBased) {
			if (state.has_value()) {
				return true;
			}
		}
		return false;
	}

	std::string OutfitService::PickRandomStateOutfit(const std::string& a_exclude) const
	{
		if (_outfits.empty()) {
			return {};
		}
		const auto& settings = Settings::Get();
		const bool  favoritesOnly = settings.randomFavoritesOnly;
		const bool  excludeCurrent = settings.randomExcludeCurrent;
		std::vector<std::string> candidates;
		candidates.reserve(_outfits.size());
		// Same candidate rules as the random hotkey (NGFeatures::PickRandomOutfit):
		// honour "favourites only" and "never pick the outfit that is already
		// worn", and judge the exclusion against the candidates that are left, not
		// against every outfit in the store - otherwise a single favourite that is
		// worn right now made the roll jump between "exclude it" and "must be it".
		const auto collect = [&](bool a_onlyFavourites, bool a_excludeCurrent) {
			candidates.clear();
			for (const auto& [key, outfit] : _outfits) {
				if (a_onlyFavourites && !outfit.favorite_c()) {
					continue;
				}
				if (a_excludeCurrent && !a_exclude.empty() && key == a_exclude) {
					continue;
				}
				candidates.push_back(key);
			}
		};
		collect(favoritesOnly, excludeCurrent);
		if (candidates.empty() && favoritesOnly) {
			// Favourites only was requested but the user has no favourite yet:
			// fall back to every outfit rather than doing nothing.
			collect(false, excludeCurrent);
		}
		if (candidates.empty() && excludeCurrent) {
			// Only one outfit exists and it is the one that is worn: rolling it
			// again beats silently doing nothing.
			collect(false, false);
		}
		if (candidates.empty()) {
			return _outfits.begin()->first;
		}
		static thread_local std::mt19937 rng{ std::random_device{}() };
		std::uniform_int_distribution<std::size_t> dist(0, candidates.size() - 1);
		return candidates[dist(rng)];
	}

	void OutfitService::set_outfit_using_state(StateType a_state, RE::FormID a_actor)
	{
		auto* assignment = FindAssignment(a_actor);
		if (!assignment) {
			return;
		}
		const auto index = static_cast<std::size_t>(a_state);
		if (index >= kStateTypeCount || !assignment->stateBased[index]) {
			return;
		}
		const bool sameState = assignment->lastState.has_value() && *assignment->lastState == static_cast<std::uint32_t>(index);
		assignment->lastState = static_cast<std::uint32_t>(index);

		const std::string& configured = *assignment->stateBased[index];
		if (Canonical(configured) == Canonical(kRandomStateOutfit)) {
			if (sameState && assignment->current) {
				// Combat events fire repeatedly; keep the outfit that was rolled
				// when the actor entered this state.
				return;
			}
			const auto picked = PickRandomStateOutfit(assignment->current.value_or(std::string()));
			if (!picked.empty() && Canonical(picked) != assignment->current.value_or(std::string())) {
				assignment->current = picked;
				// State switches change the illusion exactly like a manual
				// assignment: without the bump the hook keeps the cheap applySet
				// path and the clones of the previous outfit survive.
				++_revision;
				SKSE::log::debug("State random: actor {:08X} entered state {} and rolls '{}'.", a_actor, index, picked);
			}
			return;
		}
		if (Canonical(configured) != assignment->current.value_or(std::string())) {
			assignment->current = configured;
			++_revision;
		}
	}

	void OutfitService::set_state_outfit(StateType a_state, RE::FormID a_actor, const char* a_name)
	{
		const auto index = static_cast<std::size_t>(a_state);
		if (index >= kStateTypeCount) {
			return;
		}
		if (!a_name || !*a_name) {
			unset_state_outfit(a_state, a_actor);
			return;
		}
		auto key = Canonical(a_name);
		const auto isRandom = key == Canonical(kRandomStateOutfit);
		if (!isRandom && !_outfits.contains(key)) {
			return;
		}
		auto& assignment = _assignments[a_actor];
		assignment.stateBased[index] = key;
		// A changed configuration must be allowed to apply (and re-roll) the
		// next time this state is evaluated.
		assignment.lastState.reset();
		++_revision;
		if (isRandom) {
			// Roll right away so that picking "<random>" has a visible effect even
			// while the actor stays in the same state.
			const auto picked = PickRandomStateOutfit(assignment.current.value_or(std::string()));
			if (!picked.empty()) {
				assignment.current = picked;
			}
		}
	}

	void OutfitService::unset_state_outfit(StateType a_state, RE::FormID a_actor)
	{
		const auto index = static_cast<std::size_t>(a_state);
		if (index >= kStateTypeCount) {
			return;
		}
		auto* assignment = FindAssignment(a_actor);
		if (assignment) {
			assignment->stateBased[index].reset();
			assignment->lastState.reset();
			++_revision;
		}
	}

	rust::String OutfitService::get_state_outfit_name_c(StateType a_state, RE::FormID a_actor) const
	{
		const auto index = static_cast<std::size_t>(a_state);
		if (index >= kStateTypeCount) {
			return rust::String();
		}
		const auto* assignment = FindAssignment(a_actor);
		if (!assignment || !assignment->stateBased[index]) {
			return rust::String();
		}
		if (Canonical(*assignment->stateBased[index]) == Canonical(kRandomStateOutfit)) {
			return rust::String(kRandomStateOutfit);
		}
		if (const auto* outfit = Find(*assignment->stateBased[index])) {
			return outfit->name_c();
		}
		return rust::String();
	}

	OptionalStateType OutfitService::check_location_type_c(const rust::Vec<rust::String>& a_keywords, WeatherFlags a_weather, bool a_isInCombat, RE::FormID a_actor) const
	{
		OptionalStateType result;
		const auto* assignment = FindAssignment(a_actor);
		if (!assignment) {
			return result;
		}
		auto configured = [&](StateType a_state) {
			const auto index = static_cast<std::size_t>(a_state);
			return index < kStateTypeCount && assignment->stateBased[index].has_value();
		};
		auto pick = [&](StateType a_state) {
			result.has_value = true;
			result.value = a_state;
			return result;
		};

		const bool city = HasKeyword(a_keywords, kLocationCityKeyword);
		const bool town = HasKeyword(a_keywords, kLocationTownKeyword);
		const bool dungeon = HasKeyword(a_keywords, kLocationDungeonKeyword);

		// The priority order is fixed: combat beats interior types, and the
		// weather variants are only considered when weather is active.
		if (a_isInCombat && configured(StateType::Combat)) return pick(StateType::Combat);
		if (city && a_weather.snowy && configured(StateType::CitySnowy)) return pick(StateType::CitySnowy);
		if (city && a_weather.rainy && configured(StateType::CityRainy)) return pick(StateType::CityRainy);
		if (city && configured(StateType::City)) return pick(StateType::City);
		if (town && city && a_weather.snowy && configured(StateType::TownSnowy)) return pick(StateType::TownSnowy);
		if (town && city && a_weather.rainy && configured(StateType::TownRainy)) return pick(StateType::TownRainy);
		if (town && city && configured(StateType::Town)) return pick(StateType::Town);
		if (dungeon && a_weather.snowy && configured(StateType::DungeonSnowy)) return pick(StateType::DungeonSnowy);
		if (dungeon && a_weather.rainy && configured(StateType::DungeonRainy)) return pick(StateType::DungeonRainy);
		if (dungeon && configured(StateType::Dungeon)) return pick(StateType::Dungeon);
		if (a_weather.snowy && configured(StateType::WorldSnowy)) return pick(StateType::WorldSnowy);
		if (a_weather.rainy && configured(StateType::WorldRainy)) return pick(StateType::WorldRainy);
		if (configured(StateType::World)) return pick(StateType::World);
		return result;
	}

	bool OutfitService::IsFormIDPermitted(RE::FormID a_form)
	{
		return a_form < 0xFF000000;
	}

	void OutfitService::replace_with_new()
	{
		// Mirrors the Rust implementation: a brand new game (or a save without
		// our record) starts with no outfits at all. The player assignment is
		// re-added by check_consistency().
		std::lock_guard lock(_mutex);
		_outfits.clear();
		_assignments.clear();
		// A new game (or a save without our record) starts with overrides on.
		// The co-save loader writes the real value later when one exists.
		_enabled.store(true, std::memory_order_relaxed);
		++_revision;
	}

	void OutfitService::check_consistency()
	{
		std::lock_guard lock(_mutex);
		bool changed = false;
		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			changed = _assignments.try_emplace(player->GetFormID()).second;
		}
		for (auto it = _assignments.begin(); it != _assignments.end();) {
			if (!IsFormIDPermitted(it->first)) {
				it = _assignments.erase(it);
				changed = true;
			} else {
				++it;
			}
		}
		if (changed) {
			++_revision;
		}
	}

	rust::String OutfitService::save_json_c() const
	{
		std::lock_guard lock(_mutex);
		nlohmann::json root;
		root["version"] = 2;
		root["enabled"] = _enabled.load(std::memory_order_relaxed);
		auto& outfits = root["outfits"] = nlohmann::json::array();
		for (const auto& [key, outfit] : _outfits) {
			nlohmann::json entry;
			entry["name"] = std::string(outfit.name_c());
			entry["favorite"] = outfit.favorite_c();
			entry["blanket"] = outfit.blanket_policy();
			auto& slots = entry["slots"] = nlohmann::json::array();
			for (std::uint32_t slot = 0; slot < kSlotCount; ++slot) {
				if (outfit.slot_policy(slot).has_value()) {
					slots.push_back({ { "slot", slot }, { "code", *outfit.slot_policy(slot) } });
				}
			}
			auto& armors = entry["armors"] = nlohmann::json::array();
			for (const auto* armor : outfit.armors()) {
				if (armor) {
					armors.push_back({ { "form", armor->GetFormID() } });
				}
			}
			outfits.push_back(std::move(entry));
		}
		auto& actors = root["actors"] = nlohmann::json::array();
		for (const auto& [formId, assignment] : _assignments) {
			nlohmann::json entry;
			entry["actor"] = formId;
			entry["autoSwitch"] = assignment.stateBasedAutoSwitchingEnabled;
			if (assignment.current) {
				entry["current"] = *assignment.current;
			}
			auto& states = entry["states"] = nlohmann::json::array();
			for (std::size_t i = 0; i < kStateTypeCount; ++i) {
				if (assignment.stateBased[i]) {
					states.push_back({ { "state", i }, { "outfit", *assignment.stateBased[i] } });
				}
			}
			actors.push_back(std::move(entry));
		}
		return rust::String(root.dump());
	}

	namespace
	{
		// Looks up the first present key of a protobuf JSON object. The parser
		// writes lowerCamelCase names, but hand edited files often keep the
		// original snake_case ones, and both are valid for protobuf.
		const nlohmann::json* FetchField(const nlohmann::json& a_object, std::initializer_list<const char*> a_names)
		{
			if (!a_object.is_object()) {
				return nullptr;
			}
			for (const char* name : a_names) {
				if (const auto it = a_object.find(name); it != a_object.end()) {
					return std::addressof(*it);
				}
			}
			return nullptr;
		}

		std::string FetchString(const nlohmann::json& a_object, std::initializer_list<const char*> a_names)
		{
			const auto* field = FetchField(a_object, a_names);
			return (field && field->is_string()) ? field->get<std::string>() : std::string();
		}

		bool FetchBool(const nlohmann::json& a_object, std::initializer_list<const char*> a_names, bool a_default)
		{
			const auto* field = FetchField(a_object, a_names);
			if (!field) {
				return a_default;
			}
			if (field->is_boolean()) {
				return field->get<bool>();
			}
			if (field->is_number_integer()) {
				return field->get<long long>() != 0;
			}
			return a_default;
		}

		std::uint32_t FetchUint(const nlohmann::json& a_object, std::initializer_list<const char*> a_names, std::uint32_t a_default)
		{
			const auto* field = FetchField(a_object, a_names);
			if (!field) {
				return a_default;
			}
			try {
				if (field->is_string()) {
					return static_cast<std::uint32_t>(std::stoul(field->get<std::string>(), nullptr, 10));
				}
				if (field->is_number()) {
					return static_cast<std::uint32_t>(field->get<long long>());
				}
			} catch (const std::exception&) {
			}
			return a_default;
		}

		// slotPolicy / slotPolicies hold either the four character code (files
		// written by the rewritten 0.7 code) or the numeric id of the legacy
		// enum (files written by 0.6.x, which is what the OutfitSystemData.json
		// files shipped with existing load orders contain). Accept both.
		std::optional<std::string> FetchPolicyCode(const nlohmann::json& a_object, std::initializer_list<const char*> a_names)
		{
			const auto* field = FetchField(a_object, a_names);
			if (!field) {
				return std::nullopt;
			}
			if (field->is_number_integer()) {
				const auto value = static_cast<std::uint32_t>(field->get<long long>());
				auto       code = PolicyFromInt(value);
				if (!code) {
					SKSE::log::warn("OutfitSystemData.json: the policy '{}' is unknown; it is ignored.", value);
				}
				return code;
			}
			if (!field->is_string()) {
				return std::nullopt;
			}
			const auto raw = field->get<std::string>();
			if (raw.empty()) {
				return std::nullopt;
			}
			auto code = NormalizePolicyCode(raw);
			if (code.empty()) {
				SKSE::log::warn("OutfitSystemData.json: the policy '{}' is unknown; it is ignored.", raw);
				return std::nullopt;
			}
			return code;
		}

		// A protobuf map with uint32 keys prints as an object with decimal
		// string keys: {"9": "XEXO"}.
		std::uint32_t ParseMapKey(const std::string& a_key)
		{
			try {
				return static_cast<std::uint32_t>(std::stoul(a_key, nullptr, 10));
			} catch (const std::exception&) {
				return 0xFFFFFFFFu;
			}
		}
	}

	rust::String OutfitService::save_legacy_json_c() const
	{
		std::lock_guard lock(_mutex);

		const auto display_name = [this](const std::string& a_key) {
			const auto it = _outfits.find(a_key);
			return it == _outfits.end() ? a_key : std::string(it->second.name_c());
		};

		nlohmann::json root = nlohmann::json::object();
		// protobuf omits fields that still hold their default value; "enabled"
		// defaults to false, so it only shows up for an enabled system.
		if (_enabled) {
			root["enabled"] = true;
		}

		if (!_outfits.empty()) {
			auto& outfits = root["outfits"] = nlohmann::json::array();
			for (const auto& [key, outfit] : _outfits) {
				nlohmann::json entry = nlohmann::json::object();
				entry["name"] = std::string(outfit.name_c());
				if (outfit.favorite_c()) {
					entry["isFavorite"] = true;
				}
				if (!outfit.armors().empty()) {
					auto& armors = entry["armors"] = nlohmann::json::array();
					for (const auto* armor : outfit.armors()) {
						if (!armor) {
							continue;
						}
						nlohmann::json locator = nlohmann::json::object();
						locator["rawFormId"] = static_cast<std::uint32_t>(armor->GetRawFormID());
						locator["localFormId"] = static_cast<std::uint32_t>(armor->GetLocalFormID());
						if (const auto* file = armor->GetFile(0)) {
							const auto filename = file->GetFilename();
							if (!filename.empty()) {
								locator["modName"] = std::string(filename);
							}
						}
						armors.push_back(std::move(locator));
					}
				}
				// Write the numeric ids of the legacy enum, never a localized
				// label: that is the format of the OutfitSystemData.json files
				// written by the original mod (0.6.x) and what a round trip has
				// to reproduce. protobuf omits values that equal the default, so
				// the base policy "XXXX" (0) is left out as well.
				if (const auto blanket = PolicyToInt(NormalizePolicyCode(outfit.blanket_policy())); blanket && *blanket != 0) {
					entry["slotPolicy"] = *blanket;
				}
				nlohmann::json slots = nlohmann::json::object();
				for (std::uint32_t slot = 0; slot < kSlotCount; ++slot) {
					const auto& policy = outfit.slot_policy(slot);
					if (!policy.has_value() || policy->empty()) {
						continue;
					}
					if (const auto value = PolicyToInt(NormalizePolicyCode(*policy))) {
						slots[std::to_string(slot)] = *value;
					}
				}
				if (!slots.empty()) {
					entry["slotPolicies"] = std::move(slots);
				}
				outfits.push_back(std::move(entry));
			}
		}

		if (!_assignments.empty()) {
			auto& actors = root["actorOutfitAssignments"] = nlohmann::json::object();
			for (const auto& [formId, assignment] : _assignments) {
				if (!IsFormIDPermitted(formId)) {
					continue;
				}
				nlohmann::json entry = nlohmann::json::object();
				if (assignment.current) {
					entry["currentOutfitName"] = display_name(*assignment.current);
				}
				if (assignment.stateBasedAutoSwitchingEnabled) {
					entry["stateBasedAutoSwitchEnabled"] = true;
				}
				nlohmann::json states = nlohmann::json::object();
				for (std::size_t i = 0; i < kStateTypeCount; ++i) {
					if (assignment.stateBased[i]) {
						states[std::to_string(i)] = display_name(*assignment.stateBased[i]);
					}
				}
				if (!states.empty()) {
					entry["stateBasedOutfits"] = std::move(states);
				}
				actors[std::to_string(formId)] = std::move(entry);
			}
		}

		return rust::String(root.dump(2));
	}

	bool OutfitService::replace_with_legacy_json(rust::Str a_json, const SKSE::SerializationInterface* a_intfc)
	{
		nlohmann::json root;
		try {
			root = nlohmann::json::parse(a_json.str(), nullptr, true, true);
		} catch (const std::exception& e) {
			SKSE::log::error("OutfitSystemData.json: {}", e.what());
			return false;
		}
		if (!root.is_object()) {
			return false;
		}

		// Same transactional shape as the co-save reader: build into locals, swap
		// only once the whole file has been read without a throw.
		std::unordered_map<std::string, Outfit> loadedOutfits;
		std::unordered_map<RE::FormID, ActorAssignments> assignments;
		// proto3 default for OutfitSystem.enabled is false.
		const bool enabled = FetchBool(root, { "enabled", "Enabled" }, false);

		try {

		const auto resolve_locator = [a_intfc](const nlohmann::json& a_locator) -> RE::TESObjectARMO* {
			const auto local = FetchUint(a_locator, { "localFormId", "local_form_id" }, 0);
			const auto mod = FetchString(a_locator, { "modName", "mod_name" });
			if (local != 0 && !mod.empty()) {
				if (auto* handler = RE::TESDataHandler::GetSingleton()) {
					if (auto* form = handler->LookupForm<RE::TESObjectARMO>(local, mod)) {
						return form;
					}
				}
			}
			const auto raw = FetchUint(a_locator, { "rawFormId", "raw_form_id" }, 0);
			if (raw != 0 && a_intfc) {
				RE::FormID resolved = raw;
				if (a_intfc->ResolveFormID(raw, resolved) && resolved != 0) {
					return RE::TESForm::LookupByID<RE::TESObjectARMO>(resolved);
				}
			}
			return nullptr;
		};

		if (const auto* outfits = FetchField(root, { "outfits", "outfits" }); outfits && outfits->is_array()) {
			for (const auto& entry : *outfits) {
				if (!entry.is_object()) {
					continue;
				}
				const auto name = FetchString(entry, { "name", "name" });
				if (name.empty()) {
					continue;
				}
				Outfit outfit(name);
				outfit.set_favorite(FetchBool(entry, { "isFavorite", "is_favorite" }, false));

				if (const auto* armors = FetchField(entry, { "armors", "armors" }); armors && armors->is_array()) {
					for (const auto& locator : *armors) {
						if (locator.is_object()) {
							if (auto* armor = resolve_locator(locator)) {
								outfit.insert_armor(armor);
							}
						} else if (locator.is_number()) {
							// Some hand written files keep the plain form ids.
							const auto raw = static_cast<std::uint32_t>(locator.get<long long>());
							RE::FormID resolved = raw;
							if (a_intfc && a_intfc->ResolveFormID(raw, resolved) && resolved != 0) {
								if (auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(resolved)) {
									outfit.insert_armor(armor);
								}
							}
						}
					}
				}
				if (const auto* obsolete = FetchField(entry, { "OBSOLETEArmors", "OBSOLETE_armors" }); obsolete && obsolete->is_array()) {
					for (const auto& value : *obsolete) {
						if (!value.is_number()) {
							continue;
						}
						const auto raw = static_cast<std::uint32_t>(value.get<long long>());
						RE::FormID resolved = raw;
						if (a_intfc && a_intfc->ResolveFormID(raw, resolved) && resolved != 0) {
							if (auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(resolved)) {
								outfit.insert_armor(armor);
							}
						}
					}
				}

				// Same order as the original from_proto_data(): reset to the
				// raw defaults, apply the named policies, then the numeric ones.
				for (std::uint32_t slot = 0; slot < kSlotCount; ++slot) {
					outfit.set_slot_policy_raw(slot, std::nullopt);
				}
				outfit.set_blanket_policy_raw("XXXX");
				if (const auto* slots = FetchField(entry, { "slotPolicies", "slot_policies" }); slots && slots->is_object()) {
					for (const auto& [slotKey, value] : slots->items()) {
						const auto slot = ParseMapKey(slotKey);
						if (slot >= kSlotCount) {
							continue;
						}
						if (value.is_number()) {
							// A file that stores the legacy enum instead of the code.
							if (auto code = PolicyFromInt(static_cast<std::uint32_t>(value.get<long long>()))) {
								outfit.set_slot_policy_raw(slot, std::move(*code));
							}
							continue;
						}
						if (!value.is_string()) {
							continue;
						}
						const auto raw = value.get<std::string>();
						if (raw.empty()) {
							continue;
						}
						// 1.4.1 and older could store a localized label here; accept
						// both the code and every known label.
						const auto code = NormalizePolicyCode(raw);
						if (code.empty()) {
							SKSE::log::warn("OutfitSystemData.json: outfit '{}' slot {} has the unknown policy '{}'; the slot follows the base policy.",
								name, slot, raw);
							continue;
						}
						outfit.set_slot_policy_raw(slot, code);
					}
				}
				if (const auto blanket = FetchPolicyCode(entry, { "slotPolicy", "slot_policy" })) {
					outfit.set_blanket_policy_raw(*blanket);
				}
				if (const auto* slots = FetchField(entry, { "OBSOLETESlotPolicies", "OBSOLETE_slot_policies" }); slots && slots->is_object()) {
					for (const auto& [slotKey, value] : slots->items()) {
						const auto slot = ParseMapKey(slotKey);
						if (slot >= kSlotCount || !value.is_number()) {
							continue;
						}
						if (auto code = PolicyFromInt(static_cast<std::uint32_t>(value.get<long long>()))) {
							outfit.set_slot_policy_raw(slot, std::move(*code));
						}
					}
				}
				const auto obsoleteBlanket = FetchUint(entry, { "OBSOLETESlotPolicy", "OBSOLETE_slot_policy" }, 0);
				if (obsoleteBlanket != 0) {
					if (auto code = PolicyFromInt(obsoleteBlanket)) {
						outfit.set_blanket_policy_raw(std::move(*code));
					}
				}

				const auto key = Canonical(name);
				if (!loadedOutfits.emplace(key, std::move(outfit)).second) {
					SKSE::log::warn("OutfitSystemData.json: '{}' appears more than once; only the first copy is kept.", name);
				}
			}
		}

		if (const auto* actors = FetchField(root, { "actorOutfitAssignments", "actor_outfit_assignments" }); actors && actors->is_object()) {
			for (const auto& [actorKey, entry] : actors->items()) {
				const auto actor = ParseMapKey(actorKey);
				if (actor == 0 || actor == 0xFFFFFFFFu || !entry.is_object()) {
					continue;
				}
				ActorAssignments assignment;
				assignment.stateBasedAutoSwitchingEnabled = FetchBool(entry, { "stateBasedAutoSwitchEnabled", "state_based_auto_switch_enabled" }, false);
				const auto current = FetchString(entry, { "currentOutfitName", "current_outfit_name" });
				if (!current.empty()) {
					auto key = Canonical(current);
					if (loadedOutfits.contains(key)) {
						assignment.current = key;
					}
				}
				if (const auto* states = FetchField(entry, { "stateBasedOutfits", "state_based_outfits" }); states && states->is_object()) {
					for (const auto& [stateKey, value] : states->items()) {
						const auto state = ParseMapKey(stateKey);
						if (state >= kStateTypeCount || !value.is_string()) {
							continue;
						}
						auto key = Canonical(value.get<std::string>());
						if (loadedOutfits.contains(key) || key == Canonical(kRandomStateOutfit)) {
							assignment.stateBased[state] = key;
						}
					}
				}
				assignments[static_cast<RE::FormID>(actor)] = std::move(assignment);
			}
		}
		} catch (const std::exception& e) {
			SKSE::log::error("OutfitSystemData.json: {}", e.what());
			return false;
		}

		std::lock_guard lock(_mutex);
		_outfits = std::move(loadedOutfits);
		_assignments = std::move(assignments);
		_enabled.store(enabled, std::memory_order_relaxed);
		check_consistency();
		++_revision;
		SKSE::log::info("Imported {} outfits and {} actor assignments from OutfitSystemData.json.", _outfits.size(), _assignments.size());
		return true;
	}

	bool OutfitService::replace_with_json_data(rust::Str a_json, const SKSE::SerializationInterface* a_intfc)
	{
		nlohmann::json root;
		try {
			root = nlohmann::json::parse(a_json.str(), nullptr, true, true);
		} catch (const std::exception& e) {
			SKSE::log::error("Failed to parse save data: {}", e.what());
			return false;
		}
		if (!root.is_object()) {
			return false;
		}

		// Parse into local containers and swap in one go at the end. nlohmann's
		// value() throws on a type mismatch, and clearing the store before the
		// parse meant one bad field in an old co-save (or a hand-edited one) left
		// the player with no outfits at all.
		std::unordered_map<std::string, Outfit> loadedOutfits;
		std::unordered_map<RE::FormID, ActorAssignments> assignments;
		const bool enabled = FetchBool(root, { "enabled" }, true);

		try {
		if (const auto outfitsField = root.find("outfits"); outfitsField != root.end() && outfitsField->is_array()) {
			for (const auto& entry : *outfitsField) {
				if (!entry.is_object()) {
					continue;
				}
				auto name = FetchString(entry, { "name" });
				if (name.empty()) {
					continue;
				}
				Outfit outfit(name);
				outfit.set_favorite(FetchBool(entry, { "favorite" }, false));
				if (const auto blank = entry.find("blanket"); blank != entry.end() && blank->is_string()) {
					outfit.set_blanket_policy_raw(blank->get<std::string>());
				}
				if (const auto* slots = FetchField(entry, { "slots" }); slots && slots->is_array()) {
					for (const auto& slot : *slots) {
						if (!slot.is_object()) {
							continue;
						}
						const auto index = FetchUint(slot, { "slot" }, 0);
						const auto code = FetchString(slot, { "code" });
						if (index < kSlotCount && !code.empty()) {
							outfit.set_slot_policy_raw(index, code);
						}
					}
				}
				if (const auto* armors = FetchField(entry, { "armors" }); armors && armors->is_array()) {
					for (const auto& armorEntry : *armors) {
						if (auto* armor = ResolveArmor(armorEntry, a_intfc)) {
							outfit.insert_armor(armor);
						}
					}
				}
				const auto key = Canonical(name);
				if (!loadedOutfits.emplace(key, std::move(outfit)).second) {
					SKSE::log::warn("Co-save: '{}' appears more than once; only the first copy is kept.", name);
				}
			}
		}

		if (const auto actorsField = root.find("actors"); actorsField != root.end() && actorsField->is_array()) {
			for (const auto& entry : *actorsField) {
				if (!entry.is_object()) {
					continue;
				}
				const auto actor = FetchUint(entry, { "actor" }, 0);
				if (actor == 0) {
					continue;
				}
				ActorAssignments assignment;
				assignment.stateBasedAutoSwitchingEnabled = FetchBool(entry, { "autoSwitch" }, false);
				const auto current = FetchString(entry, { "current" });
				if (!current.empty()) {
					auto key = Canonical(current);
					if (loadedOutfits.contains(key)) {
						assignment.current = key;
					}
				}
				if (const auto* states = FetchField(entry, { "states" }); states && states->is_array()) {
					for (const auto& state : *states) {
						if (!state.is_object()) {
							continue;
						}
						const auto index = FetchUint(state, { "state" }, 0);
						auto key = Canonical(FetchString(state, { "outfit" }));
						if (index < kStateTypeCount && (loadedOutfits.contains(key) || key == Canonical(kRandomStateOutfit))) {
							assignment.stateBased[index] = key;
						}
					}
				}
				assignments[static_cast<RE::FormID>(actor)] = std::move(assignment);
			}
		}
		} catch (const std::exception& e) {
			// Nothing has been swapped in yet, so the store the player already has
			// survives a broken save instead of being replaced by an empty one.
			SKSE::log::error("Failed to read save data: {}", e.what());
			return false;
		}

		std::lock_guard lock(_mutex);
		_outfits = std::move(loadedOutfits);
		_assignments = std::move(assignments);
		_enabled.store(enabled, std::memory_order_relaxed);
		check_consistency();
		++_revision;
		SKSE::log::info("Loaded {} outfits and {} actor assignments from the co-save.", _outfits.size(), _assignments.size());
		return true;
	}

	OutfitServiceReadGuard::OutfitServiceReadGuard()
	{
		OutfitService::Get()._mutex.lock();
	}

	OutfitServiceReadGuard::~OutfitServiceReadGuard()
	{
		OutfitService::Get()._mutex.unlock();
	}

	OutfitServiceWriteGuard::OutfitServiceWriteGuard()
	{
		OutfitService::Get()._mutex.lock();
	}

	OutfitServiceWriteGuard::~OutfitServiceWriteGuard()
	{
		OutfitService::Get()._mutex.unlock();
	}

	OutfitServiceReadGuard outfit_service_get_singleton_ptr()
	{
		return OutfitServiceReadGuard{};
	}

	OutfitServiceWriteGuard outfit_service_get_mut_singleton_ptr()
	{
		return OutfitServiceWriteGuard{};
	}
}// namespace SOS
