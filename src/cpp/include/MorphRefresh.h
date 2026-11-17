#pragma once

#include <cstdint>

#include <RE/Skyrim.h>

namespace RE
{
	class Actor;
}

namespace SOS::MorphRefresh
{
	// Acquires RaceMenu's BodyMorph interface, OBody's preset API and SLIF NG's
	// query interface (any combination). Called once from kDataLoaded. The
	// providers supply the signal that the actor's shape changed; without at
	// least one of them the module stays inactive.
	void Install();

	// Tries the providers again. RaceMenu does not always answer the interface
	// exchange at kDataLoaded: on 1.5.97 skee only starts listening from inside
	// one of its own message handlers, so the answer can be minutes late. This
	// runs from the heartbeat (fast at first, then every 30 s for the whole
	// session), from kPostLoadGame and lazily from the poll right before the
	// interface is used. It never gives up.
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
	// and RaceMenu's morph pass skips them; this gives every illusion slot its
	// .tri path and re-applies the morphs the actor currently holds. Runs from a
	// main-thread task and does not detach or rebuild anything.
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
