#include "compat/ModCompat.h"

#include "compat/AndCompat.h"
#include "compat/PadCompat.h"

#include "core/Settings.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <chrono>

namespace SOS::Compat
{
	namespace
	{
		Status g_status;
	}

	bool Install()
	{
		const auto& settings = Settings::Get();

		g_status.andDetected = And::Detected();
		g_status.padDetected = Pad::Detected();

		if (g_status.andDetected) {
			g_status.andHooked = And::Install();
			g_status.andVersion = And::Version();
		}
		if (g_status.padDetected) {
			Pad::ReportStatus();
			g_status.padVersion = Pad::Version();
		}

		SKSE::log::debug("Compatibility: Advanced Nudity Detection = {}, Procedural Armor Damage = {} (toggles: AND {}, PAD {}).",
			g_status.andDetected ? And::StatusText() : "not installed",
			g_status.padDetected ? Pad::StatusText() : "not installed",
			settings.compatAdvancedNudityDetection ? "on" : "off",
			settings.compatProceduralArmorDamage ? "on" : "off");
		return g_status.andHooked;
	}

	void Reload()
	{
		if (!g_status.andDetected && And::Detected()) {
			g_status.andDetected = true;
			g_status.andHooked = And::Install();
			g_status.andVersion = And::Version();
		}
		g_status.padDetected = Pad::Detected();
		if (g_status.padDetected && g_status.padVersion.empty()) {
			g_status.padVersion = Pad::Version();
		}
		SKSE::log::debug("Compatibility: reloaded (AND {}, PAD {}).", And::StatusText(), Pad::StatusText());
	}

	const Status& GetStatus()
	{
		return g_status;
	}

	void OnOutfitApplied(RE::Actor* a_actor)
	{
		And::Sync(a_actor);
		Pad::OnOutfitApplied(a_actor);
	}

	void Tick()
	{
		static std::chrono::steady_clock::time_point last{};
		const auto now = std::chrono::steady_clock::now();
		if (now - last < std::chrono::seconds(1)) {
			return;
		}
		last = now;

		if (!g_status.andDetected) {
			return;
		}
		// The input sink runs on the device manager's thread; hop to the main
		// thread before touching actor data.
		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks) {
			return;
		}
		tasks->AddTask([]() {
			And::Sync(RE::PlayerCharacter::GetSingleton());
		});
	}
}
