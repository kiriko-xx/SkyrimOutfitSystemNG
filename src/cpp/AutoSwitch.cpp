#include "AutoSwitch.h"

#include "OutfitSystem.h"
#include "core/OutfitService.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace SOS::AutoSwitch
{
	namespace
	{
		// FormID -> StateType value applied last (-1 = no configured state).
		std::unordered_map<RE::FormID, int> g_lastState;
		std::mutex                           g_cacheMutex;
		bool                                 g_installed{ false };
		std::atomic<bool>                    g_tickPending{ false };

		void Evaluate(RE::Actor* a_actor)
		{
			if (!a_actor) {
				return;
			}
			const auto actorId = a_actor->GetFormID();
			std::optional<StateType> state;
			bool                     eligible = false;
			{
				auto       service = outfit_service_get_singleton_ptr();
				const auto& store = service->inner();
				// should_override() requires a base outfit, but state switching is
				// also useful when the actor only has state outfits configured.
				eligible = store.enabled_c() && store.get_state_based_switching_enabled_c(actorId) && (store.should_override(actorId) || store.has_any_state_outfit(actorId));
				if (eligible) {
					state = OutfitSystem::identifyStateType(store, a_actor);
				}
			}

			const int value = state ? static_cast<int>(*state) : -1;
			{
				std::lock_guard lock(g_cacheMutex);
				const auto      it = g_lastState.find(actorId);
				if (!eligible) {
					if (it != g_lastState.end()) {
						g_lastState.erase(it);
					}
					return;
				}
				if (it != g_lastState.end() && it->second == value) {
					return;
				}
				g_lastState[actorId] = value;
			}

			if (!state) {
				return;
			}
			{
				auto service = outfit_service_get_mut_singleton_ptr();
				OutfitSystem::setOutfitUsingState(service->inner(), a_actor);
			}
			// The refresh needs the outfit lock released.
			OutfitSystem::refreshArmorFor(a_actor);
			SKSE::log::debug("AutoSwitch: {:08X} switched to state {}.", actorId, value);
		}

		class LocationSink final : public RE::BSTEventSink<RE::TESActorLocationChangeEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESActorLocationChangeEvent* a_event, RE::BSTEventSource<RE::TESActorLocationChangeEvent>*) override
			{
				if (a_event && a_event->actor) {
					Evaluate(a_event->actor->As<RE::Actor>());
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class FastTravelSink final : public RE::BSTEventSink<RE::TESFastTravelEndEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESFastTravelEndEvent*, RE::BSTEventSource<RE::TESFastTravelEndEvent>*) override
			{
				Evaluate(RE::PlayerCharacter::GetSingleton());
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		LocationSink   g_locationSink;
		FastTravelSink g_fastTravelSink;
	}

	bool Install()
	{
		if (g_installed) {
			return true;
		}
		auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
		if (!holder) {
			SKSE::log::warn("AutoSwitch: the script event source is not available; location switching falls back to the input heartbeat.");
			return false;
		}
		holder->AddEventSink<RE::TESActorLocationChangeEvent>(&g_locationSink);
		holder->AddEventSink<RE::TESFastTravelEndEvent>(&g_fastTravelSink);
		g_installed = true;
		SKSE::log::info("AutoSwitch: native location and fast travel detection installed.");
		return true;
	}

	void TickOnMainThread()
	{
		std::vector<RE::FormID> actors;
		{
			auto service = outfit_service_get_singleton_ptr();
			for (const auto formId : service->inner().list_actors()) {
				actors.push_back(formId);
			}
		}
		const std::unordered_set<RE::FormID> live(actors.begin(), actors.end());
		{
			// Drop cached states of actors that were unassigned or whose reference
			// is gone, otherwise a long session slowly accumulates dead FormIDs.
			std::lock_guard lock(g_cacheMutex);
			for (auto it = g_lastState.begin(); it != g_lastState.end();) {
				if (live.contains(it->first)) {
					++it;
				} else {
					it = g_lastState.erase(it);
				}
			}
		}
		for (const auto formId : actors) {
			if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(formId)) {
				Evaluate(actor);
			}
		}
	}

	void Tick()
	{
		// The input sink runs on the device manager's thread. All actor/form
		// access below must happen on the main thread, so only schedule work here.
		static auto last = std::chrono::steady_clock::time_point{};
		const auto  now = std::chrono::steady_clock::now();
		if (last.time_since_epoch().count() != 0 && now - last < std::chrono::seconds(2)) {
			return;
		}
		if (g_tickPending.exchange(true, std::memory_order_acq_rel)) {
			return;
		}
		last = now;

		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks) {
			g_tickPending.store(false, std::memory_order_release);
			return;
		}
		tasks->AddTask([]() {
			g_tickPending.store(false, std::memory_order_release);
			TickOnMainThread();
		});
	}

	void Reset()
	{
		std::lock_guard lock(g_cacheMutex);
		g_lastState.clear();
	}
}
