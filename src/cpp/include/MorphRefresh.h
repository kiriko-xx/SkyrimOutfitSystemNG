#pragma once

#include <cstdint>

#include <RE/Skyrim.h>

namespace RE
{
	class Actor;
}

namespace SOS::MorphRefresh
{
	// Called at DataLoaded. Readable BODYTRI paths alone do not guarantee that
	// older RaceMenu versions visit illusion nodes; explicit replay needs its API.
	void Install();

	// PostPostLoad probe only; no actor, mesh or event-sink work.
	void ProbeRaceMenuInterface();

	// Main-thread provider discovery and event installation. Never call from
	// skinning callbacks; retries cannot repair a missing sender subscription.
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

	// Runs after every outfit change (menu, quick picker, hotkey, Papyrus, scene
	// start, scene end). The caller already triggered one equip pass; this adds a
	// bounded series of follow-up passes, because a single pass fills the biped
	// but does not materialise the clone the game draws - the menu path only ever
	// worked because closing the menu pokes the equip manager repeatedly.
	void RequestOutfitPasses(RE::Actor* a_actor);

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
