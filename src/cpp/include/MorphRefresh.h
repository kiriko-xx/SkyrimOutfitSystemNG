#pragma once

#include <cstdint>

#include <RE/Skyrim.h>

namespace RE
{
	class Actor;
}

namespace SOS::MorphRefresh
{
	// Arms the .tri morph targets on every illusion clone and asks the body
	// morph providers for their interfaces (RaceMenu, OBody, SLIF NG - any
	// combination). Called once from kDataLoaded. The .tri targets work without
	// any interface: RaceMenu and OBody morph the clones in their own apply
	// pass. The interfaces only add the immediate re-morph and the morph
	// callback.
	void Install();

	// Tries the providers again. Must run on the main thread, outside any SKSE
	// message handler and outside the skinning hooks: the handshake is a
	// message dispatch, and skse64.log shows a dispatch sent from inside a
	// handler only reaches the plugins that registered a catch-all listener
	// late (so skee, loaded early, never answers). Callers queue it through the
	// task interface; the heartbeat retries forever, fast at first and then
	// every 30 s.
	void RetryProviders();

	// Forgets every signature and pending rebuild (new game / load).
	void Reset();

	// Cheap heartbeat from the input sink. It schedules a main-thread poll at
	// most four times per second; the poll is where the actual provider calls
	// happen.
	void Tick();

	// True while the illusion slots of this actor must be detached and rebuilt
	// from scratch so that a fresh .tri morph target is loaded.
	bool ShouldForceRebuild(RE::FormID a_actor);

	// Called by the skinning hook after it rebuilt an actor. Queues one
	// ApplyBodyMorphs pass shortly after the rebuild, when the deferred
	// partClone has actually been attached.
	void NotifyRebuilt(RE::Actor* a_actor);

	// Called by the skinning hook once it applied the display set. The clones it
	// creates never pass skee's attach hook, so they carry no .tri morph target
	// and RaceMenu's own morph pass skips them; this gives every illusion slot
	// its .tri path and, when the interface is available, re-applies the morphs
	// the actor currently holds. Runs from a main-thread task and does not
	// detach or rebuild anything - and it runs with or without the interface.
	void NotifySlotApplied(RE::Actor* a_actor);

	// Takes the illusion off, lets the engine run one pass over the real armour
	// and puts the illusion straight back on - the same sequence a user does by
	// hand (clear the illusion, then wear the outfit again), but with both
	// refreshes inside one main-thread task so no frame ever shows the real
	// gear. This is the only sequence that makes the engine load a fresh .tri
	// morph target for every display slot. Returns false when the actor has no
	// illusion to cycle.
	bool RunIllusionCycle(RE::Actor* a_actor, const char* a_reason);

	// Same as RunIllusionCycle, for call sites that phrase it as a re-application.
	void ScheduleReillusion(RE::Actor* a_actor);

	bool Available();
}
