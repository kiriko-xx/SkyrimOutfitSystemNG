#pragma once

#include <cstdint>

namespace SOS::HairTintDiagnostics
{
	// Call on the main thread after UI is available; registration is idempotent.
	void Install();

	// Pass only the player's current illusion mask, before any morph writes.
	void SampleOnMainThread(std::uint32_t a_coveredSlotMask);

	// Main thread only; keeps event sinks registered across loads.
	void Reset();
}
