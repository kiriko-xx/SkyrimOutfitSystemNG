// Slot rules: one piece per body slot, with FX (bit 31) excluded from the
// comparison. Compiled by build/slot_rules_test/run.bat against src/.

#include "core/SlotRules.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace
{
	int g_failures = 0;

	void Check(bool a_condition, const char* a_what)
	{
		if (!a_condition) {
			std::printf("FAIL %s\n", a_what);
			++g_failures;
		}
	}

	void CheckMasks(const std::vector<std::uint32_t>& a_masks,
		const std::vector<std::size_t>& a_expectedKept,
		const std::vector<std::size_t>& a_expectedDropped,
		const char* a_what)
	{
		std::vector<std::size_t> kept;
		std::vector<std::size_t> dropped;
		SOS::KeepOnePiecePerSlot(a_masks, kept, dropped);
		Check(kept == a_expectedKept, a_what);
		if (kept != a_expectedKept) {
			std::printf("  kept:");
			for (const auto index : kept) {
				std::printf(" %zu", index);
			}
			std::printf("\n");
		}
		Check(dropped == a_expectedDropped, a_what);
		if (dropped != a_expectedDropped) {
			std::printf("  dropped:");
			for (const auto index : dropped) {
				std::printf(" %zu", index);
			}
			std::printf("\n");
		}
	}
}

int main()
{
	using SOS::BodySlotsOf;
	using SOS::SlotsCollide;

	// ---- bit 31 is the FX channel, not a body slot -----------------------
	Check(BodySlotsOf(SOS::kFxSlotBit) == 0, "fx bit has no body slot");
	Check(!SlotsCollide(SOS::kFxSlotBit, SOS::kFxSlotBit), "two fx pieces do not collide");
	Check(!SlotsCollide(SOS::kFxSlotBit, 0x1u), "fx does not collide with head");
	Check(BodySlotsOf(0x00000001u | SOS::kFxSlotBit) == 0x1u, "mixed piece keeps its body slot only");

	// ---- plain body slot collisions --------------------------------------
	Check(SlotsCollide(0x1u, 0x1u), "same slot collides");
	Check(!SlotsCollide(0x1u, 0x2u), "different slots do not collide");
	Check(SlotsCollide(0x1002u, 0x1000u), "multi slot piece collides on a shared slot");
	Check(!SlotsCollide(0x1002u, 0x0004u), "multi slot piece misses an unrelated slot");

	// ---- first piece of a slot wins --------------------------------------
	CheckMasks({ 0x1u, 0x1u, 0x2u }, { 0, 2 }, { 1 }, "first of a slot wins");
	CheckMasks({}, {}, {}, "empty list");
	CheckMasks({ SOS::kFxSlotBit, SOS::kFxSlotBit }, { 0, 1 }, {}, "fx only pieces are all kept");
	CheckMasks({ 0x1u, SOS::kFxSlotBit, 0x1u }, { 0, 1 }, { 2 }, "fx piece survives next to a plain one");

	// ---- the shapes seen in real plugins ---------------------------------
	// [BAKU]DOAXVV Love Letter Maiden: five upper body branches plus one of the
	// two panties. The panties are the same slot, so only one may go in.
	CheckMasks({ 0x80u, 0x10u, 0x10000u, 0x10000u }, { 0, 1, 2 }, { 3 }, "panty alternative dropped");
	// Personal Pack: a dress covers body + calves, a stocking only calves. They
	// share the calf slot, so only the first one stays; the whole piece is dropped
	// because that is what the editor already does when a slot is taken.
	CheckMasks({ 0x00000004u | 0x00000100u, 0x00000100u }, { 0 }, { 1 }, "dress eats the stocking slot");
	CheckMasks({ 0x00000100u, 0x00000004u | 0x00000100u }, { 0 }, { 1 }, "order decides who keeps the shared slot");
	// A hood that also covers the ears cannot join a helmet in the head slot.
	CheckMasks({ 0x00000001u, 0x00000001u | 0x00000002u }, { 0 }, { 1 }, "helmet and hood share the head slot");
	CheckMasks({ 0x00000001u | 0x00000002u, 0x00000002u }, { 0 }, { 1 }, "ears already covered");
	// FX alone never blocks anything, so a mixed piece can follow a plain one.
	CheckMasks({ 0x00000002u, 0x00000004u | SOS::kFxSlotBit }, { 0, 1 }, {}, "fx does not eat a free slot");

	if (g_failures == 0) {
		std::printf("SLOT_RULES_OK\n");
		return 0;
	}
	std::printf("SLOT_RULES_FAILED %d\n", g_failures);
	return 1;
}
