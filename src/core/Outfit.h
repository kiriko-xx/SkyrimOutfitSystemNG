#pragma once

#include "core/SlotRules.h"
#include "core/Types.h"

#include <array>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace RE
{
	class TESObjectARMO;
}

namespace SOS
{
	// Number of biped slots (BOD2 editor indices 30..61 map to bits 0..31).
	inline constexpr std::uint32_t kSlotCount = 32;

	struct PolicyDef;

	class Outfit
	{
	public:
		explicit Outfit(std::string a_name);

		[[nodiscard]] rust::String name_c() const { return _name; }
		[[nodiscard]] bool favorite_c() const { return _favorite; }
		void set_favorite(bool a_favorite) { _favorite = a_favorite; }
		void set_name(std::string a_name) { _name = std::move(a_name); }

		[[nodiscard]] std::vector<TESObjectARMOPtr> armors_c() const;
		[[nodiscard]] const std::set<const RE::TESObjectARMO*>& armors() const { return _armors; }

		void insert_armor(const RE::TESObjectARMO* a_armor);
		void erase_armor(const RE::TESObjectARMO* a_armor);
		void erase_all_armors();

		[[nodiscard]] bool conflicts_with(const RE::TESObjectARMO* a_armor) const;

		// Pieces already in the outfit that cover one of the body slots of
		// \a a_armor. Empty means the piece can go in without a slot clash.
		// FX (bit 31) is not a body slot, see core/SlotRules.h.
		[[nodiscard]] std::vector<const RE::TESObjectARMO*> slot_conflicts(const RE::TESObjectARMO* a_armor) const;

		// Adds the piece only when none of its body slots is taken yet, so an
		// outfit never holds two pieces of the same slot. Returns whether the
		// piece was added; the blocking pieces are appended to \a a_conflicts
		// when it is not null, which lets the caller offer to replace them.
		bool insert_armor_slot_checked(const RE::TESObjectARMO* a_armor, std::vector<const RE::TESObjectARMO*>* a_conflicts = nullptr);

		// The heart of the illusion: decide what to actually draw on the actor.
		[[nodiscard]] std::vector<TESObjectARMOPtr> compute_display_set_c(rust::Slice<const RE::TESObjectARMO*> a_equipped) const;

		// One entry per biped slot (translated policy name, or the "inherit"
		// key when the slot has no override) plus one trailing entry for the
		// blanket policy, which is what the MCM reads at index 32.
		[[nodiscard]] std::vector<rust::String> policy_names_for_outfit() const;

		void set_slot_policy_c(std::uint32_t a_slot, const char* a_code);
		void set_blanket_slot_policy_c(const char* a_code);
		void reset_to_default_slot_policy();

		[[nodiscard]] const std::optional<std::string>& slot_policy(std::uint32_t a_slot) const { return _slotPolicies[a_slot]; }
		[[nodiscard]] const std::string& blanket_policy() const { return _blanketPolicy; }
		void set_slot_policy_raw(std::uint32_t a_slot, std::optional<std::string> a_code);
		void set_blanket_policy_raw(std::string a_code)
		{
			_blanketPolicy = std::move(a_code);
			_policiesDirty = true;
		}

	private:
		// Cached slot -> armour map. Rebuilt lazily after the armour set changes;
		// compute_display_set_c() runs from the skinning hooks, so avoiding a
		// full scan of the set on every call matters.
		void EnsureOutfitSlots() const;
		void EnsurePolicyCache() const;

		std::string _name;
		std::set<const RE::TESObjectARMO*> _armors;
		bool _favorite{ false };
		mutable std::array<const RE::TESObjectARMO*, kSlotCount> _outfitBySlot{};
		mutable bool _outfitSlotsDirty{ true };
		// Resolved pointers into the static policy table. Computing the display
		// set runs for every worn-armour visitor, so a linear search over all
		// twelve policies for each of the 32 slots adds up quickly.
		mutable std::array<const PolicyDef*, kSlotCount> _resolvedPolicies{};
		mutable const PolicyDef* _resolvedBlanket{ nullptr };
		mutable bool _policiesDirty{ true };
		std::array<std::optional<std::string>, kSlotCount> _slotPolicies{};
		std::string _blanketPolicy{ "XXOO" };
	};

	// Applies the one-piece-per-slot rule to a whole list: the first piece of a
	// slot wins and later pieces that only cover slots already taken are
	// dropped. Dropped pieces are appended to \a a_dropped when it is not null.
	// Used by every bulk add path (preset creation, paste, overwrite, import).
	[[nodiscard]] std::vector<const RE::TESObjectARMO*> KeepSlotUnique(const std::vector<const RE::TESObjectARMO*>& a_armors,
		std::vector<const RE::TESObjectARMO*>* a_dropped = nullptr);
}// namespace SOS
