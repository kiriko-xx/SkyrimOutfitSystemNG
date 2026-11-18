#include "SceneOutfits.h"

#include "OutfitSystem.h"
#include "core/OutfitService.h"
#include "core/Settings.h"

#include <RE/Skyrim.h>
#include <SKSE/Events.h>
#include <SKSE/SKSE.h>
#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace SOS::SceneOutfits
{
	namespace
	{
		bool                  g_installed{ false };
		bool                  g_active{ false };
		std::set<std::string> g_activeSources;
		std::string           g_previousOutfit;
		std::string           g_sceneOutfit;

		// SexLab and OStim can report in the same frame and the switch itself can
		// re-enter this file (the outfit refresh is a game call), so the switch runs
		// under a recursive lock.
		std::recursive_mutex g_switchMutex;

		// Script events do not always arrive on the game thread: in practice OStim
		// and SexLab hand them over from a worker thread. The switch itself has to
		// run on the game thread (same place the location change the state
		// switching reacts to is handled), so an off-thread event is handed over
		// with a single task -- the way AutoSwitch::Tick reaches the game thread.
		bool OnGameThread()
		{
			auto* main = RE::Main::GetSingleton();
			return main && main->threadID != 0 && main->threadID == GetCurrentThreadId();
		}

		// Runs on the game thread only: the outfit store, the player and the
		// refresh all belong there.
		void Dispatch(const std::string& a_event);

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

		// The whole switch, done the way the location and combat switching does it:
		// take the store, assign the name, release the store, then refresh the biped.
		// There are no scene specific rules and no extra passes.
		bool SwitchTo(RE::Actor* a_actor, const std::string& a_name)
		{
			{
				auto service = outfit_service_get_mut_singleton_ptr();
				if (!OutfitSystem::setOutfitByName(service->inner(), a_actor, a_name.c_str())) {
					return false;
				}
			}
			// The refresh needs the outfit lock released.
			OutfitSystem::refreshArmorFor(a_actor);
			return true;
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

			// The scene outfit is only a name: "<current>_sex". Look it up exactly
			// first, then ignoring case, so a differently cased entry still resolves.
			std::string candidate = current + "_sex";
			{
				auto service = outfit_service_get_singleton_ptr();
				if (!service->inner().has_outfit(candidate.c_str())) {
					std::string found;
					const std::string wanted = ToLower(candidate);
					std::vector<std::string> names;
					for (const auto& name : service->inner().get_outfit_names(false)) {
						names.emplace_back(name);
					}
					for (const auto& name : names) {
						if (ToLower(name) == wanted) {
							found = name;
							break;
						}
					}
					if (found.empty()) {
						SKSE::log::info("SceneOutfits: no '{}' outfit for the {} scene; create an outfit with exactly that name to use scene switching. The current outfit is kept.", candidate, a_source);
						return;
					}
					candidate = found;
				}
			}

			g_previousOutfit = current;
			g_sceneOutfit = candidate;
			g_active = true;
			g_activeSources.insert(a_source);
			if (!SwitchTo(player, candidate)) {
				SKSE::log::warn("SceneOutfits: '{}' could not be assigned for the {} scene; the current outfit is kept.", candidate, a_source);
				g_previousOutfit.clear();
				g_sceneOutfit.clear();
				g_active = false;
				g_activeSources.erase(a_source);
				return;
			}
			SKSE::log::info("SceneOutfits: {} scene started; '{}' -> '{}'.", a_source, current, candidate);
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

			if (!SwitchTo(player, previous)) {
				SKSE::log::warn("SceneOutfits: '{}' could not be restored after the {} scene.", previous, a_source);
				return;
			}
			SKSE::log::info("SceneOutfits: {} scene ended; '{}' restored.", a_source, previous);
		}

		void Dispatch(const std::string& a_event)
		{
			std::lock_guard switchLock(g_switchMutex);
			if (a_event == "ostim_start") {
				StartScene("OStim");
			} else if (a_event == "ostim_end") {
				EndScene("OStim");
			} else if (a_event == "PlayerAnimationStart") {
				StartScene("SexLab");
			} else if (a_event == "PlayerAnimationEnd") {
				EndScene("SexLab");
			}
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
				if (name != "ostim_start" && name != "ostim_end" && name != "PlayerAnimationStart" && name != "PlayerAnimationEnd") {
					return RE::BSEventNotifyControl::kContinue;
				}
				const std::string eventName(name);
				if (!OnGameThread()) {
					auto* main = RE::Main::GetSingleton();
					SKSE::log::debug("SceneOutfits: '{}' came from thread {} (game thread {}); the switch is handed to the game thread.", eventName, GetCurrentThreadId(), main ? main->threadID : 0);
					auto* tasks = SKSE::GetTaskInterface();
					if (!tasks) {
						SKSE::log::warn("SceneOutfits: '{}' came from another thread and there is no task interface, so the switch cannot be handed over; the scene outfit is left as it is.", eventName);
						return RE::BSEventNotifyControl::kContinue;
					}
					tasks->AddTask([eventName]() { Dispatch(eventName); });
					return RE::BSEventNotifyControl::kContinue;
				}
				// On the game thread the switch runs right here, the way the location
				// sink switches an outfit from inside the event it was handed.
				Dispatch(eventName);
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		ModEventSink g_sink;
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
		std::lock_guard switchLock(g_switchMutex);
		g_active = false;
		g_activeSources.clear();
		g_previousOutfit.clear();
		g_sceneOutfit.clear();
	}
}
