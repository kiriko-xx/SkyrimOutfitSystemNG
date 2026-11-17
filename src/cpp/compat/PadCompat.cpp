#include "compat/PadCompat.h"

#include "compat/DisplaySet.h"
#include "compat/ModuleInfo.h"

#include "core/Settings.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <Windows.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace SOS::Compat::Pad
{
	namespace
	{
		std::string g_version;
		bool g_reported{ false };

		// Slots that a visible piece covers without a real armour underneath.
		// Remembered per actor so the summary is written once per change instead
		// of on every refresh (the outfit is re-applied on every equipment change).
		std::unordered_map<RE::FormID, std::uint32_t> g_orphanMask;

		std::uint32_t OrphanMask(RE::Actor* a_actor, const std::vector<TESObjectARMOPtr>& a_display)
		{
			std::vector<const RE::TESObjectARMO*> worn;
			if (!CollectWornArmors(a_actor, worn)) {
				return 0;
			}
			std::uint32_t wornMask = 0;
			for (const auto* armor : worn) {
				wornMask |= SlotMaskOf(armor);
			}

			std::uint32_t orphan = 0;
			for (const auto& item : a_display) {
				if (item.ptr) {
					orphan |= SlotMaskOf(item.ptr) & ~wornMask;
				}
			}
			return orphan;
		}
	}

	bool Detected()
	{
		return ::GetModuleHandleW(L"ProceduralArmorDamage.dll") != nullptr;
	}

	std::string Version()
	{
		return g_version;
	}

	const char* StatusText()
	{
		if (!Detected()) {
			return "not installed";
		}
		return Settings::Get().compatProceduralArmorDamage ? "detected (illusion meshes are torn as the real armour)" : "detected, compatibility disabled";
	}

	void ReportStatus()
	{
		if (g_reported || !Detected()) {
			return;
		}
		g_reported = true;

		g_version = ModuleVersion(::GetModuleHandleW(L"ProceduralArmorDamage.dll"));
		SKSE::log::debug("Compatibility: Procedural Armor Damage {} detected.", g_version.empty() ? "(unknown version)" : g_version);
		SKSE::log::debug("Compatibility: the outfit reuses the real armour's biped entry, so PAD tears the mesh that is drawn while the damage stays on the real armour.");
		SKSE::log::debug("Compatibility: a slot the outfit covers without any real armour underneath has no item for PAD to damage. Use the 'Log slot report' button in the settings menu to list those slots.");
	}

	void OnOutfitApplied(RE::Actor* a_actor)
	{
		// This map only exists to suppress repeated diagnostic lines. Bound it so
		// a long session that visits many actors cannot grow it without limit.
		if (g_orphanMask.size() > 512) {
			g_orphanMask.clear();
		}

		if (!a_actor || !Detected() || !Settings::Get().compatProceduralArmorDamage) {
			return;
		}

		std::vector<TESObjectARMOPtr> display;
		if (!ComputeDisplaySet(a_actor, display)) {
			return;
		}

		const auto orphan = OrphanMask(a_actor, display);
		const auto id = a_actor->GetFormID();
		const auto it = g_orphanMask.find(id);
		const bool changed = it == g_orphanMask.end() || it->second != orphan;
		if (changed) {
			g_orphanMask[id] = orphan;
			if (orphan == 0) {
				SKSE::log::debug("PAD compatibility: {} now shows armour over slots that all have a real armour underneath; every visible piece can be damaged.", a_actor->GetName());
			} else {
				SKSE::log::debug("PAD compatibility: {} shows pieces over slot mask 0x{:X} with no real armour underneath. Procedural Armor Damage has nothing to damage there - wear a real armour in those slots or accept that the piece stays intact.", a_actor->GetName(), orphan);
			}
		}

		if (!Settings::Get().extraLogging) {
			return;
		}
		for (const auto& item : display) {
			if (item.ptr) {
				SKSE::log::debug("PAD compatibility: {} shows '{}' (slot mask 0x{:X}).", a_actor->GetName(), item.ptr->GetName(), SlotMaskOf(item.ptr));
			}
		}
	}

	void Diagnose(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return;
		}

		const auto* module = ::GetModuleHandleW(L"ProceduralArmorDamage.dll");
		SKSE::log::info("PAD slot report for '{}' ({:08X}): PAD {}, compatibility {}.",
			a_actor->GetName(), a_actor->GetFormID(),
			module ? (Version().empty() ? "detected" : Version().c_str()) : "not installed",
			Settings::Get().compatProceduralArmorDamage ? "enabled" : "disabled");

		std::vector<const RE::TESObjectARMO*> worn;
		CollectWornArmors(a_actor, worn);
		std::uint32_t wornMask = 0;
		for (const auto* armor : worn) {
			wornMask |= SlotMaskOf(armor);
			SKSE::log::info("PAD slot report: real worn armour '{}' (slot mask 0x{:X}).", armor->GetName(), SlotMaskOf(armor));
		}
		if (worn.empty()) {
			SKSE::log::info("PAD slot report: '{}' wears no real armour at all, so Procedural Armor Damage has nothing to track.", a_actor->GetName());
		}

		std::vector<TESObjectARMOPtr> display;
		if (!ComputeDisplaySet(a_actor, display)) {
			SKSE::log::info("PAD slot report: no outfit is applied to '{}'.", a_actor->GetName());
			return;
		}

		for (std::uint32_t slot = 0; slot < RE::BIPED_OBJECTS::kEditorTotal; ++slot) {
			const std::uint32_t mask = 1u << slot;
			const RE::TESObjectARMO* shown = nullptr;
			for (const auto& item : display) {
				if (item.ptr && (SlotMaskOf(item.ptr) & mask)) {
					shown = item.ptr;
					break;
				}
			}
			if (!shown) {
				continue;
			}
			const auto* real = a_actor->GetWornArmor(static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(mask), true);
			SKSE::log::info("PAD slot report: slot {:2d} (0x{:X}) shown '{}' | real '{}'{}",
				slot, mask, shown->GetName(), real ? real->GetName() : "<none>",
				real ? "" : "  -> nothing for Procedural Armor Damage to damage");
		}
	}
}
