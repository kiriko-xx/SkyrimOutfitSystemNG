#include "HairTintRecovery.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/spdlog.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>

namespace SOS::HairTintRecovery
{
	namespace
	{
		constexpr std::size_t kMaxRoots = 8;
		constexpr std::size_t kMaxObjects = 512;
		constexpr std::size_t kMaxGeometries = 64;
		constexpr unsigned kMaxDepth = 64;
		std::atomic<std::uintptr_t> g_pending{ 0 };
		bool g_installed = false;
		unsigned g_skipLogged = 0;

		class NodeSink final : public RE::BSTEventSink<SKSE::NiNodeUpdateEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::NiNodeUpdateEvent* a_event,
				RE::BSTEventSource<SKSE::NiNodeUpdateEvent>*) override
			{
				if (a_event) {
					g_pending.store(reinterpret_cast<std::uintptr_t>(a_event->reference), std::memory_order_relaxed);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		enum class Skip : unsigned { kContext, kRoots, kSource, kBudget, kOverlap, kChanged, kNoTargets };

		void DebugSkip(Skip a_reason, const char* a_text, std::size_t a_sampled = 0)
		{
			const auto bit = 1u << static_cast<unsigned>(a_reason);
			if (!(g_skipLogged & bit)) {
				g_skipLogged |= bit;
				SKSE::log::debug("HairTintRecovery: eventConsumed=1 sourceSampled={} corrected=0 skip={}", a_sampled, a_text);
			}
		}

		template <class T, std::size_t N>
		bool Seen(const std::array<T*, N>& a_values, std::size_t a_count, T* a_value)
		{
			for (std::size_t i = 0; i < a_count; ++i) {
				if (a_values[i] == a_value) { return true; }
			}
			return false;
		}

		bool Finite(const RE::NiColor& a_color)
		{
			return std::isfinite(a_color.red) && std::isfinite(a_color.green) && std::isfinite(a_color.blue);
		}

		RE::BSShaderMaterial* Material(RE::BSGeometry* a_geometry)
		{
			auto* shader = a_geometry->GetGeometryRuntimeData().shaderProperty.get();
			auto* lighting = shader ? netimmerse_cast<RE::BSLightingShaderProperty*>(shader) : nullptr;
			return lighting ? lighting->material : nullptr;
		}

		RE::BSLightingShaderMaterialHairTint* HairTint(RE::BSShaderMaterial* a_material)
		{
			if (!a_material || a_material->GetType() != RE::BSShaderMaterial::Type::kLighting ||
				a_material->GetFeature() != RE::BSShaderMaterial::Feature::kHairTint) { return nullptr; }
			return static_cast<RE::BSLightingShaderMaterialHairTint*>(a_material);
		}

		struct Roots
		{
			std::array<RE::NiAVObject*, kMaxRoots> values{};
			std::size_t count = 0;

			bool Collect(RE::Actor* a_player, std::uint32_t a_mask)
			{
				auto* source = a_player->GetHeadPartObject(RE::BGSHeadPart::HeadPartType::kHair);
				if (!source) { return false; }
				values[count++] = source;
				for (const bool firstPerson : { false, true }) {
					const auto biped = a_player->GetBiped(firstPerson);
					if (!biped) { continue; }
					for (std::uint32_t slot = 0; slot < 32; ++slot) {
						if (!(a_mask & (1u << slot))) { continue; }
						auto* root = biped->objects[slot].partClone.get();
						if (!root) { continue; }
						if (root == source) { return false; }
						if (Seen(values, count, root)) { continue; }
						if (count == kMaxRoots) { return false; }
						values[count++] = root;
					}
				}
				return true;
			}
		};

		struct Budget
		{
			std::size_t objects = 0;
			std::size_t edges = 0;
			std::size_t geometries = 0;
			bool truncated = false;
		};

		struct Scan
		{
			Budget& budget;
			std::array<RE::NiAVObject*, kMaxObjects> seen{};
			std::size_t objects = 0;
			std::array<RE::BSGeometry*, kMaxGeometries> geometry{};
			std::array<RE::BSShaderMaterial*, kMaxGeometries> material{};
			std::size_t geometries = 0;
			RE::NiAVObject* forbiddenRoot = nullptr;
			bool overlap = false;

			void Visit(RE::NiAVObject* a_object, unsigned a_depth = 0)
			{
				if (!a_object || budget.truncated) { return; }
				if (a_object == forbiddenRoot) { overlap = true; return; }
				if (Seen(seen, objects, a_object)) { return; }
				if (budget.objects == kMaxObjects || a_depth >= kMaxDepth) { budget.truncated = true; return; }
				++budget.objects;
				seen[objects++] = a_object;
				if (auto* geom = a_object->AsGeometry()) {
					if (budget.geometries == kMaxGeometries) { budget.truncated = true; return; }
					++budget.geometries;
					geometry[geometries] = geom;
					material[geometries++] = Material(geom);
					return;
				}
				if (auto* node = a_object->AsNode()) {
					for (const auto& child : node->GetChildren()) {
						if (budget.edges == kMaxObjects) { budget.truncated = true; return; }
						++budget.edges;
						Visit(child.get(), a_depth + 1);
						if (budget.truncated) { return; }
					}
				}
			}
		};

		bool Context(RE::Actor* a_player)
		{
			if (!a_player || a_player != RE::PlayerCharacter::GetSingleton()) { return false; }
			auto* ui = RE::UI::GetSingleton();
			return ui && ui->IsMenuOpen("RaceSex Menu") && a_player->Is3DLoaded();
		}
	}

	void Install()
	{
		static NodeSink sink;
		if (!g_installed) {
			if (auto* source = SKSE::GetNiNodeUpdateEventSource()) {
				source->AddEventSink(&sink);
				g_installed = true;
			}
		}
	}

	void Reset()
	{
		g_pending.store(0, std::memory_order_relaxed);
		g_skipLogged = 0;
	}

	void Drain(RE::Actor* a_player, std::uint32_t a_coveredSlotMask)
	{
		const auto identity = g_pending.exchange(0, std::memory_order_relaxed);
		if (!identity) { return; }
		// Never dereference the event's borrowed reference, even on the main thread.
		if (!a_player || identity != reinterpret_cast<std::uintptr_t>(static_cast<RE::TESObjectREFR*>(a_player))) { return; }
		if (!a_coveredSlotMask || !Context(a_player)) { DebugSkip(Skip::kContext, "context"); return; }

		Roots roots;
		if (!roots.Collect(a_player, a_coveredSlotMask)) { DebugSkip(Skip::kRoots, "source/roots/budget"); return; }
		Budget budget;
		Scan source{ budget };
		source.Visit(roots.values[0]);
		if (budget.truncated) { DebugSkip(Skip::kBudget, "source-budget"); return; }
		RE::NiColor color;
		bool haveColor = false;
		std::size_t sampled = 0;
		for (std::size_t i = 0; i < source.geometries; ++i) {
			if (const auto* hair = HairTint(source.material[i])) {
				++sampled;
				const auto sample = hair->tintColor;
				if (!Finite(sample) || (haveColor && color != sample)) { DebugSkip(Skip::kSource, "ambiguous/nonfinite-source", sampled); return; }
				color = sample;
				haveColor = true;
			}
		}
		if (!haveColor) { DebugSkip(Skip::kSource, "missing-source-color"); return; }

		Scan targets{ budget };
		targets.forbiddenRoot = roots.values[0];
		for (std::size_t i = 1; i < roots.count; ++i) { targets.Visit(roots.values[i]); }
		if (budget.truncated) { DebugSkip(Skip::kBudget, "target-budget", sampled); return; }
		if (targets.overlap) { DebugSkip(Skip::kOverlap, "source-root-in-target", sampled); return; }

		// Reacquire current roots before committing; all borrowed scene pointers die with this task.
		Roots current;
		if (!Context(a_player) || !current.Collect(a_player, a_coveredSlotMask) || current.count != roots.count || current.values != roots.values) {
			DebugSkip(Skip::kChanged, "roots-changed", sampled); return;
		}
		for (std::size_t i = 0; i < source.geometries; ++i) {
			if (Material(source.geometry[i]) != source.material[i]) { DebugSkip(Skip::kChanged, "source-material-changed", sampled); return; }
			if (const auto* hair = HairTint(source.material[i]); hair && hair->tintColor != color) { DebugSkip(Skip::kChanged, "source-color-changed", sampled); return; }
		}
		for (std::size_t i = 0; i < targets.geometries; ++i) {
			if (Material(targets.geometry[i]) != targets.material[i]) { DebugSkip(Skip::kChanged, "target-material-changed", sampled); return; }
		}

		std::array<RE::BSShaderMaterial*, kMaxGeometries> written{};
		std::size_t count = 0;
		for (std::size_t i = 0; i < targets.geometries; ++i) {
			auto* material = Material(targets.geometry[i]);
			if (Seen(source.geometry, source.geometries, targets.geometry[i]) ||
				Seen(source.material, source.geometries, material) || Seen(written, count, material)) { continue; }
			auto* hair = HairTint(material);
			if (!hair || hair->tintColor == color) { continue; }
			// CL-defined render material, not a third-party layout. Unknown aliases outside
			// these bounded roots remain a sharing risk; known source aliases are excluded.
			hair->tintColor = color;
			written[count++] = material;
		}
		if (count) {
			SKSE::log::info("HairTintRecovery: eventConsumed=1 sourceSampled={} restored count={} source=({},{},{})", sampled, count, color.red, color.green, color.blue);
		} else {
			DebugSkip(Skip::kNoTargets, "no-distinct-target-needing-recovery", sampled);
		}
	}
}
