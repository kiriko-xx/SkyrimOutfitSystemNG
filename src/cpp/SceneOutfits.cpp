#include "SceneOutfits.h"

#include "OutfitSystem.h"
#include "core/OutfitService.h"
#include "core/Settings.h"

#include <RE/Skyrim.h>
#include <SKSE/API.h>
#include <SKSE/Events.h>
#include <SKSE/SKSE.h>
#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <deque>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <utility>

namespace SOS::SceneOutfits
{
	namespace
	{
		bool                    g_installed{ false };
		bool                    g_active{ false };
		std::set<std::string>   g_activeSources;
		std::string             g_previousOutfit;
		std::string             g_sceneOutfit;

		std::string ToLower(std::string_view a_text)
		{
			std::string result(a_text);
			std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
				return static_cast<char>(std::tolower(c));
			});
			return result;
		}

		bool EndsWithSexSuffix(std::string_view a_name)
		{
			return a_name.size() >= 4 && ToLower(a_name.substr(a_name.size() - 4)) == "_sex";
		}

		std::string CurrentOutfitName(RE::Actor* a_actor)
		{
			auto service = outfit_service_get_singleton_ptr();
			const auto* outfit = service->inner().current_outfit_ptr(a_actor->GetFormID());
			return outfit ? outfit->name_c() : std::string();
		}

		void StartScene(const char* a_source)
		{
			if (!Settings::Get().sceneOutfitSwitching) {
				return;
			}
			// OStim and SexLab can overlap. Keep one source per scene so the
			// previous outfit is only restored when the last scene ends.
			if (g_activeSources.contains(a_source)) {
				return;
			}
			if (g_active) {
				g_activeSources.insert(a_source);
				SKSE::log::debug("SceneOutfits: {} scene started while another scene is active; it will share the current scene outfit.", a_source);
				return;
			}
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return;
			}

			const std::string current = CurrentOutfitName(player);
			if (current.empty()) {
				return;
			}
			if (EndsWithSexSuffix(current)) {
				SKSE::log::debug("SceneOutfits: '{}' already ends with _sex; it is kept for the {} scene.", current, a_source);
				return;
			}

			const std::string candidate = current + "_sex";
			{
				auto service = outfit_service_get_singleton_ptr();
				if (!service->inner().has_outfit(candidate.c_str())) {
					SKSE::log::info("SceneOutfits: no '{}' outfit for the {} scene; create an outfit with exactly that name to use scene switching. The current outfit is kept.", candidate, a_source);
					return;
				}
			}

			g_previousOutfit = current;
			g_sceneOutfit = candidate;
			g_active = true;
			g_activeSources.insert(a_source);
			{
				auto service = outfit_service_get_mut_singleton_ptr();
				service->inner().set_outfit_c(candidate.c_str(), player->GetFormID());
			}
			OutfitSystem::refreshArmorFor(player);
			SKSE::log::debug("SceneOutfits: {} scene started; '{}' -> '{}'.", a_source, current, candidate);
		}

		void EndScene(const char* a_source)
		{
			if (!g_active) {
				return;
			}
			g_activeSources.erase(a_source);
			if (!g_activeSources.empty()) {
				SKSE::log::debug("SceneOutfits: {} scene ended while another scene is still active; the scene outfit is kept.", a_source);
				return;
			}
			g_active = false;
			const std::string previous = std::move(g_previousOutfit);
			const std::string sceneOutfit = std::move(g_sceneOutfit);
			g_previousOutfit.clear();
			g_sceneOutfit.clear();

			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return;
			}
			const std::string current = CurrentOutfitName(player);
			if (ToLower(current) != ToLower(sceneOutfit)) {
				SKSE::log::debug("SceneOutfits: {} scene ended, but the player changed to '{}'; the previous outfit is not restored.", a_source, current);
				return;
			}

			{
				auto service = outfit_service_get_mut_singleton_ptr();
				service->inner().set_outfit_c(previous.c_str(), player->GetFormID());
			}
			OutfitSystem::refreshArmorFor(player);
			SKSE::log::debug("SceneOutfits: {} scene ended; '{}' restored.", a_source, previous);
		}

		enum class SceneEvent
		{
			kOStimStart,
			kOStimEnd,
			kSexLabStart,
			kSexLabEnd
		};

		std::atomic<std::uint64_t> g_generation{ 0 };
		std::mutex g_queueMutex;
		std::mutex g_dispatchMutex;
		std::deque<SceneEvent> g_pendingEvents;
		bool g_drainScheduled{ false };

		void DispatchSceneEvent(SceneEvent a_event)
		{
			switch (a_event) {
			case SceneEvent::kOStimStart:
				StartScene("OStim");
				break;
			case SceneEvent::kOStimEnd:
				EndScene("OStim");
				break;
			case SceneEvent::kSexLabStart:
				StartScene("SexLab");
				break;
			case SceneEvent::kSexLabEnd:
				EndScene("SexLab");
				break;
			}
		}

		void DrainSceneEvents(std::uint64_t a_generation)
		{
			for (;;) {
				// SKSE AddTask defers to its task pool; it does not guarantee a
				// main thread or serial Run calls. Serialize scene state ourselves.
				std::lock_guard dispatchLock(g_dispatchMutex);
				SceneEvent event;
				{
					std::lock_guard queueLock(g_queueMutex);
					if (a_generation != g_generation.load(std::memory_order_acquire)) {
						return;  // Never alter a newer generation's scheduled flag.
					}
					if (g_pendingEvents.empty()) {
						g_drainScheduled = false;
						return;
					}
					auto* main = RE::Main::GetSingleton();
					if (!main || main->threadID == 0 || main->threadID != GetCurrentThreadId()) {
						g_drainScheduled = false;
						return;  // Keep FIFO for the next input heartbeat; never spin-resubmit.
					}
					event = g_pendingEvents.front();
					g_pendingEvents.pop_front();
				}
				// Release queue lock before game calls: a callback may enqueue again.
				DispatchSceneEvent(event);
			}
		}

		void QueueSceneEvent(SceneEvent a_event)
		{
			const auto generation = g_generation.load(std::memory_order_acquire);
			auto* tasks = SKSE::GetTaskInterface();
			if (!tasks) {
				SKSE::log::warn("SceneOutfits: the task interface is not available; scene event dropped (no synchronous fallback).");
				return;
			}
			{
				std::lock_guard queueLock(g_queueMutex);
				if (generation != g_generation.load(std::memory_order_acquire)) {
					return;
				}
				// FIFO is our queue-lock acquisition order, not SKSE Run order.
				// Keep every start/end value; never coalesce overlapping source ends.
				g_pendingEvents.push_back(a_event);
				if (g_drainScheduled) {
					return;
				}
				g_drainScheduled = true;
			}
			// Capture only the owned token, never callback strings/events/actors.
			tasks->AddTask([generation]() { DrainSceneEvents(generation); });
		}

		class ModEventSink final : public RE::BSTEventSink<SKSE::ModCallbackEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*) override
			{
				if (!a_event || !a_event->eventName.c_str() || a_event->eventName.c_str()[0] == '\0') {
					return RE::BSEventNotifyControl::kContinue;
				}
				const std::string_view name = a_event->eventName.c_str();
				if (name == "ostim_start") {
					QueueSceneEvent(SceneEvent::kOStimStart);
				} else if (name == "ostim_end") {
					QueueSceneEvent(SceneEvent::kOStimEnd);
				} else if (name == "PlayerAnimationStart") {
					QueueSceneEvent(SceneEvent::kSexLabStart);
				} else if (name == "PlayerAnimationEnd") {
					QueueSceneEvent(SceneEvent::kSexLabEnd);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		ModEventSink g_sink;
	}

	void Tick()
	{
		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks) {
			return;  // Event submission already warns when the interface is absent.
		}
		const auto generation = g_generation.load(std::memory_order_acquire);
		{
			std::lock_guard queueLock(g_queueMutex);
			if (generation != g_generation.load(std::memory_order_acquire) ||
				g_pendingEvents.empty() || g_drainScheduled) {
				return;
			}
			g_drainScheduled = true;
		}
		tasks->AddTask([generation]() { DrainSceneEvents(generation); });
	}

	bool Install()
	{
		if (g_installed) {
			return true;
		}
		auto* source = SKSE::GetModCallbackEventSource();
		if (!source) {
			SKSE::log::warn("SceneOutfits: the mod callback event source is not available; scene switching is disabled.");
			return false;
		}
		source->AddEventSink<SKSE::ModCallbackEvent>(&g_sink);
		g_installed = true;
		SKSE::log::info("SceneOutfits: SexLab / OStim scene detection installed.");
		return true;
	}

	void Reset()
	{
		// Invalidate queued drains first; wait for an in-flight dispatch before
		// clearing scene state. Keep the existing Reset caller/lifecycle contract.
		g_generation.fetch_add(1, std::memory_order_acq_rel);
		std::lock_guard dispatchLock(g_dispatchMutex);
		std::lock_guard queueLock(g_queueMutex);
		g_pendingEvents.clear();
		g_drainScheduled = false;
		g_active = false;
		g_activeSources.clear();
		g_previousOutfit.clear();
		g_sceneOutfit.clear();
	}
}
