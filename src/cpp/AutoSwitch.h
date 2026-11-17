#pragma once

namespace SOS::AutoSwitch
{
	// Registers the native location / fast travel event sinks. Returns false when
	// the script event source is not available; the input-driven Tick() still
	// works as a fallback.
	bool Install();

	// Cheap poll used as a weather / missed-event safety net. Throttled to one
	// evaluation every two seconds and called from the input sink.
	void Tick();

	// Drops the cached per-actor state (new game, load, revert).
	void Reset();
}
