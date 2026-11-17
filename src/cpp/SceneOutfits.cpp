#include "SceneOutfits.h"

#include "OutfitSystem.h"
#include "core/OutfitService.h"
#include "core/Settings.h"

#include <RE/Skyrim.h>
#include <SKSE/API.h>
#include <SKSE/Events.h>
#include <SKSE/SKSE.h>

#include <algorithm>
#include <cctype>
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
					StartScene("OStim");
				} else if (name == "ostim_end") {
					EndScene("OStim");
				} else if (name == "PlayerAnimationStart") {
					StartScene("SexLab");
				} else if (name == "PlayerAnimationEnd") {
					EndScene("SexLab");
				}
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
		g_active = false;
		g_activeSources.clear();
		g_previousOutfit.clear();
		g_sceneOutfit.clear();
	}
}
