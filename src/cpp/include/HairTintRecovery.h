#pragma once

#include <cstdint>

namespace RE
{
	class Actor;
}

namespace SOS::HairTintRecovery
{
	// Main thread only; registers a static sink once the public SKSE source exists.
	void Install();

	// Call in the next main-thread task, with the current player and illusion slot mask.
	// The sink only publishes identity; the caller owns task scheduling (no fixed delay).
	// Consumes pending events even outside RaceSex Menu; no event means no scene writes.
	void Drain(RE::Actor* a_player, std::uint32_t a_coveredSlotMask);

	// Main thread only; discard pending identity, retaining the installed sink.
	void Reset();
}
