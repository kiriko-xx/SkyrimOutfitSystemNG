#include "compat/AndCompat.h"

#include "compat/DisplaySet.h"
#include "compat/ModuleInfo.h"

#include "core/Settings.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <Windows.h>

#include <string>
#include <vector>

namespace SOS::Compat::And
{
	namespace
	{
		// Biped object bits, see RE/B/BipedObjects.h. Anything that covers the
		// torso counts as "top", anything that covers the pelvis or the legs
		// counts as "bottom".
		constexpr std::uint32_t kTopMask =
			(1u << RE::BIPED_OBJECTS::kBody) |
			(1u << RE::BIPED_OBJECTS::kModChestPrimary) |
			(1u << RE::BIPED_OBJECTS::kModChestSecondary);
		constexpr std::uint32_t kBottomMask =
			(1u << RE::BIPED_OBJECTS::kBody) |
			(1u << RE::BIPED_OBJECTS::kModPelvisPrimary) |
			(1u << RE::BIPED_OBJECTS::kModPelvisSecondary) |
			(1u << RE::BIPED_OBJECTS::kModLegRight) |
			(1u << RE::BIPED_OBJECTS::kModLegLeft);

		RE::TESFaction* g_nude{ nullptr };
		RE::TESFaction* g_topless{ nullptr };
		RE::TESFaction* g_bottomless{ nullptr };
		std::string g_version;
		bool g_reported{ false };

		bool LookupFactions()
		{
			g_nude = RE::TESForm::LookupByEditorID<RE::TESFaction>("AND_NudeActorFaction");
			g_topless = RE::TESForm::LookupByEditorID<RE::TESFaction>("AND_ToplessFaction");
			g_bottomless = RE::TESForm::LookupByEditorID<RE::TESFaction>("AND_BottomlessFaction");
			return g_nude && g_topless && g_bottomless;
		}

		void SetRankIfNeeded(RE::Actor* a_actor, RE::TESFaction* a_faction, std::int32_t a_rank)
		{
			if (!a_faction) {
				return;
			}
			const bool isPlayer = a_actor->IsPlayerRef();
			if (a_actor->GetFactionRank(a_faction, isPlayer) == a_rank) {
				return;
			}
			// Same engine call Papyrus Actor.SetFactionRank ends up in.
			a_actor->AddToFaction(a_faction, static_cast<std::int8_t>(a_rank));
			if (Settings::Get().extraLogging) {
				SKSE::log::debug("AND compatibility: {} -> {} rank {}.", a_actor->GetName(), a_faction->GetFormEditorID(), a_rank);
			}
		}
	}

	bool Detected()
	{
		return ::GetModuleHandleW(L"AdvancedNudityDetection.dll") != nullptr;
	}

	std::string Version()
	{
		return g_version;
	}

	bool Hooked()
	{
		return g_nude && g_topless && g_bottomless;
	}

	const char* StatusText()
	{
		if (!Detected()) {
			return "not installed";
		}
		if (!Settings::Get().compatAdvancedNudityDetection) {
			return "detected, compatibility disabled";
		}
		return Hooked() ? "detected (nudity factions follow the outfit)" : "detected, its factions were not found";
	}

	bool Install()
	{
		if (g_reported) {
			return Hooked();
		}
		g_reported = true;

		g_version = ModuleVersion(::GetModuleHandleW(L"AdvancedNudityDetection.dll"));
		if (!LookupFactions()) {
			SKSE::log::warn("Compatibility: Advanced Nudity Detection {} was found, but its nudity factions are missing; the outfit keeps the vanilla detection.",
				g_version.empty() ? "(unknown version)" : g_version);
			return false;
		}
		SKSE::log::debug("Compatibility: Advanced Nudity Detection {} detected; nudity, topless and bottomless ranks now follow the displayed outfit.",
			g_version.empty() ? "(unknown version)" : g_version);
		return true;
	}

	void Reload()
	{
		if (Detected() && !Hooked()) {
			LookupFactions();
		}
	}

	void Sync(RE::Actor* a_actor)
	{
		if (!a_actor || !Detected() || !Settings::Get().compatAdvancedNudityDetection || !Hooked()) {
			return;
		}
		if (!OverrideActive(a_actor)) {
			return;
		}

		std::vector<TESObjectARMOPtr> display;
		if (!ComputeDisplaySet(a_actor, display)) {
			return;
		}

		std::uint32_t mask = 0;
		for (const auto& item : display) {
			if (item.ptr) {
				mask |= SlotMaskOf(item.ptr);
			}
		}

		const std::int32_t topRank = (mask & kTopMask) != 0 ? 0 : 1;
		const std::int32_t bottomRank = (mask & kBottomMask) != 0 ? 0 : 1;
		const std::int32_t nudeRank = (mask & (1u << RE::BIPED_OBJECTS::kBody)) != 0 ? 0 : 1;

		SetRankIfNeeded(a_actor, g_topless, topRank);
		SetRankIfNeeded(a_actor, g_bottomless, bottomRank);
		SetRankIfNeeded(a_actor, g_nude, nudeRank);
	}
}
