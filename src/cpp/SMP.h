#pragma once

namespace RE
{
	class Actor;
}

namespace SOS::SMP
{
	// True when Faster HDT-SMP registered its DynamicHDT Papyrus class.
	[[nodiscard]] bool IsAvailable();

	// Resets the physics of one actor after its outfit changed. The actual work
	// is queued on the game thread and calls DynamicHDT.ResetPhysics(actor, false),
	// which only touches the skeletons whose form id matches the actor.
	void ResetActor(RE::Actor* a_actor);
}
