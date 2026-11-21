#pragma once

// One piece per slot.
//
// Every path that puts armour into an outfit has to obey the same rule: a body
// biped slot may be covered by exactly one piece, otherwise which piece wins is
// an accident of set iteration order. The rule lives here without any engine
// dependency so that tests/slot_rules.cpp can compile it with a plain compiler.
//
// Bit 31 (Creation Kit slot 61, "FX") is not a body slot: FX-only pieces do not
// collide with each other and the ESP matcher already ignores the bit when it
// groups pieces. Slot checks therefore compare the mask with the FX bit
// stripped, so a mixed piece that also carries FX still wins its body slots.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace SOS
{
	inline constexpr std::uint32_t kFxSlotBit = 1u << 31;
	inline constexpr std::uint32_t kBodySlotBits = kFxSlotBit - 1u;

	[[nodiscard]] inline std::uint32_t BodySlotsOf(std::uint32_t a_mask)
	{
		return a_mask & kBodySlotBits;
	}

	[[nodiscard]] inline bool SlotsCollide(std::uint32_t a_lhs, std::uint32_t a_rhs)
	{
		return (BodySlotsOf(a_lhs) & BodySlotsOf(a_rhs)) != 0;
	}

	// Walks the masks in order and keeps the first piece of every slot; a later
	// piece that only re-covers slots already taken is dropped. A piece without
	// any body slot (FX only) never collides and is always kept.
	inline void KeepOnePiecePerSlot(const std::vector<std::uint32_t>& a_masks,
		std::vector<std::size_t>& a_kept,
		std::vector<std::size_t>& a_dropped)
	{
		a_kept.clear();
		a_dropped.clear();
		a_kept.reserve(a_masks.size());
		std::uint32_t occupied = 0;
		for (std::size_t i = 0; i < a_masks.size(); ++i) {
			const auto body = BodySlotsOf(a_masks[i]);
			if (body != 0 && (body & occupied) != 0) {
				a_dropped.push_back(i);
				continue;
			}
			occupied |= body;
			a_kept.push_back(i);
		}
	}
}  // namespace SOS
