#include "core/Outfit.h"

#include "core/Policy.h"
#include "core/Settings.h"

#include <RE/Skyrim.h>

#include <algorithm>

namespace SOS
{
	namespace
	{
		void AssignBySlot(const RE::TESObjectARMO* const* a_armors, std::size_t a_count, std::array<const RE::TESObjectARMO*, kSlotCount>& a_out)
		{
			a_out.fill(nullptr);
			for (std::size_t i = 0; i < a_count; ++i) {
				const auto* armor = a_armors[i];
				if (!armor) {
					continue;
				}
				const auto mask = armor->GetSlotMask().underlying();
				for (std::uint32_t slot = 0; slot < kSlotCount; ++slot) {
					if ((mask & (1u << slot)) != 0) {
						// Match the original implementation: later armours overwrite
						// earlier ones when their slot masks overlap.
						a_out[slot] = armor;
					}
				}
			}
		}
	}

	Outfit::Outfit(std::string a_name) :
		_name(std::move(a_name))
	{
		reset_to_default_slot_policy();
	}

	std::vector<TESObjectARMOPtr> Outfit::armors_c() const
	{
		std::vector<TESObjectARMOPtr> result;
		result.reserve(_armors.size());
		for (const auto* armor : _armors) {
			result.push_back(TESObjectARMOPtr{ armor });
	}
		return result;
	}

	void Outfit::insert_armor(const RE::TESObjectARMO* a_armor)
	{
		if (a_armor && _armors.insert(a_armor).second) {
			_outfitSlotsDirty = true;
		}
	}

	void Outfit::erase_armor(const RE::TESObjectARMO* a_armor)
	{
		if (_armors.erase(a_armor) != 0) {
			_outfitSlotsDirty = true;
		}
	}

	void Outfit::erase_all_armors()
	{
		if (!_armors.empty()) {
			_armors.clear();
			_outfitSlotsDirty = true;
		}
	}

	void Outfit::EnsureOutfitSlots() const
	{
		if (!_outfitSlotsDirty) {
			return;
		}
		_outfitBySlot.fill(nullptr);
		for (const auto* armor : _armors) {
			if (!armor) {
				continue;
			}
			const auto mask = armor->GetSlotMask().underlying();
			for (std::uint32_t slot = 0; slot < kSlotCount; ++slot) {
				if ((mask & (1u << slot)) != 0) {
					// Last wins, matching the Rust HashSet iteration semantics of
					// the original mod (later armours replace earlier ones).
					_outfitBySlot[slot] = armor;
				}
			}
		}
		_outfitSlotsDirty = false;
	}

	void Outfit::EnsurePolicyCache() const
	{
		if (!_policiesDirty) {
			return;
		}
		for (std::uint32_t slot = 0; slot < kSlotCount; ++slot) {
			const auto& code = _slotPolicies[slot].has_value() ? *_slotPolicies[slot] : _blanketPolicy;
			_resolvedPolicies[slot] = FindPolicy(code);
		}
		_resolvedBlanket = FindPolicy(_blanketPolicy);
		if (!_resolvedBlanket) {
			_resolvedBlanket = FindPolicy("XXOO");
		}
		_policiesDirty = false;
	}

	std::vector<const RE::TESObjectARMO*> Outfit::slot_conflicts(const RE::TESObjectARMO* a_armor) const
	{
		std::vector<const RE::TESObjectARMO*> conflicts;
		if (!a_armor) {
			return conflicts;
		}
		const auto mask = a_armor->GetSlotMask().underlying();
		for (const auto* armor : _armors) {
			// A piece never conflicts with itself, so re-adding one that is already
			// in the outfit stays a no-op instead of looking like a slot clash.
			if (armor && armor != a_armor && SlotsCollide(armor->GetSlotMask().underlying(), mask)) {
				conflicts.push_back(armor);
			}
		}
		return conflicts;
	}

	bool Outfit::conflicts_with(const RE::TESObjectARMO* a_armor) const
	{
		if (!a_armor) {
			return false;
		}
		const auto mask = a_armor->GetSlotMask().underlying();
		for (const auto* armor : _armors) {
			if (armor && SlotsCollide(armor->GetSlotMask().underlying(), mask)) {
				return true;
			}
		}
		return false;
	}

	bool Outfit::insert_armor_slot_checked(const RE::TESObjectARMO* a_armor, std::vector<const RE::TESObjectARMO*>* a_conflicts)
	{
		if (!a_armor) {
			return false;
		}
		auto conflicts = slot_conflicts(a_armor);
		if (!conflicts.empty()) {
			if (a_conflicts) {
				a_conflicts->insert(a_conflicts->end(), conflicts.begin(), conflicts.end());
			}
			return false;
		}
		insert_armor(a_armor);
		return true;
	}

	std::vector<const RE::TESObjectARMO*> KeepSlotUnique(const std::vector<const RE::TESObjectARMO*>& a_armors,
		std::vector<const RE::TESObjectARMO*>* a_dropped)
	{
		std::vector<std::uint32_t> masks;
		masks.reserve(a_armors.size());
		for (const auto* armor : a_armors) {
			masks.push_back(armor ? armor->GetSlotMask().underlying() : 0u);
		}
		std::vector<std::size_t> kept;
		std::vector<std::size_t> dropped;
		KeepOnePiecePerSlot(masks, kept, dropped);
		std::vector<const RE::TESObjectARMO*> result;
		result.reserve(kept.size());
		for (const auto index : kept) {
			if (a_armors[index]) {
				result.push_back(a_armors[index]);
			}
		}
		if (a_dropped) {
			for (const auto index : dropped) {
				// A null entry was not a piece at all, so it is not worth reporting.
				if (a_armors[index]) {
					a_dropped->push_back(a_armors[index]);
				}
			}
		}
		return result;
	}

	std::vector<TESObjectARMOPtr> Outfit::compute_display_set_c(rust::Slice<const RE::TESObjectARMO*> a_equipped) const
	{
		std::array<const RE::TESObjectARMO*, kSlotCount> equippedBySlot{};
		AssignBySlot(a_equipped.data(), a_equipped.size(), equippedBySlot);
		EnsureOutfitSlots();
		EnsurePolicyCache();

		const auto& settings = Settings::Get();
		std::uint32_t mask = 0;
		std::vector<TESObjectARMOPtr> result;
		result.reserve(std::min<std::size_t>(_armors.size(), kSlotCount));
		for (std::uint32_t slot = 0; slot < kSlotCount; ++slot) {
			if ((mask & (1u << slot)) != 0) {
				continue;  // already covered by an item chosen for an earlier slot
			}
			const PolicyDef* policy = _resolvedPolicies[slot];
			if (!policy) {
				// Unknown policy names used to hide the slot completely, which made
				// an imported file look like every item had vanished. Fall back to
				// the outfit wide policy, then to the always-show default.
				policy = _resolvedBlanket;
				if (!policy) {
					continue;
				}
			}
			const auto* equipped = equippedBySlot[slot];
			const auto* outfitItem = _outfitBySlot[slot];
			const RE::TESObjectARMO* choice = nullptr;
			if (equipped && settings.IsGoldenItem(equipped)) {
				choice = equipped;  // golden items always win
			} else {
				choice = EvaluatePolicy(*policy, equipped, outfitItem);
			}
			if (!choice) {
				continue;
			}
			mask |= choice->GetSlotMask().underlying();
			if (std::find_if(result.begin(), result.end(), [choice](const TESObjectARMOPtr& a_item) { return a_item.ptr == choice; }) == result.end()) {
				result.push_back(TESObjectARMOPtr{ choice });
			}
		}
		return result;
	}

	std::vector<rust::String> Outfit::policy_names_for_outfit() const
	{
		std::vector<rust::String> result;
		result.reserve(kSlotCount + 1);
		for (std::uint32_t slot = 0; slot < kSlotCount; ++slot) {
			if (_slotPolicies[slot].has_value()) {
				result.emplace_back(TranslationKey(*_slotPolicies[slot]));
			} else {
				result.emplace_back("$SkyOutSys_Desc_PolicyName_INHERIT");
			}
		}
		result.emplace_back(TranslationKey(_blanketPolicy));
		return result;
	}

	void Outfit::set_slot_policy_c(std::uint32_t a_slot, const char* a_code)
	{
		if (a_slot >= kSlotCount) {
			return;
		}
		if (!a_code || !*a_code) {
			_slotPolicies[a_slot].reset();
			_policiesDirty = true;
			return;
		}
		// Papyrus and imported files may pass a localized label; store the code.
		const auto code = NormalizePolicyCode(a_code);
		_slotPolicies[a_slot] = code.empty() ? std::string(a_code) : code;
		_policiesDirty = true;
	}

	void Outfit::set_blanket_slot_policy_c(const char* a_code)
	{
		if (!a_code || !*a_code) {
			reset_to_default_slot_policy();
			return;
		}
		const auto code = NormalizePolicyCode(a_code);
		_blanketPolicy = code.empty() ? std::string(a_code) : code;
		_policiesDirty = true;
	}

	void Outfit::reset_to_default_slot_policy()
	{
		const auto& settings = Settings::Get();
		for (auto& policy : _slotPolicies) {
			policy.reset();
		}
		_blanketPolicy = settings.defaultBasePolicy;
		if (settings.setDefaultSlotPolicies) {
			// The shield slot defaults to "passthrough" so that a shield that is
			// actually equipped keeps showing even when the outfit has none.
			_slotPolicies[9] = std::string("XEXO");
		}
		_policiesDirty = true;
	}

	void Outfit::set_slot_policy_raw(std::uint32_t a_slot, std::optional<std::string> a_code)
	{
		if (a_slot < kSlotCount) {
			_slotPolicies[a_slot] = std::move(a_code);
			_policiesDirty = true;
		}
	}
}// namespace SOS
