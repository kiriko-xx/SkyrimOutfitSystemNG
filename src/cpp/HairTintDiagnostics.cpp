#include "HairTintDiagnostics.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/spdlog.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace SOS::HairTintDiagnostics
{
	namespace
	{
		constexpr std::size_t kMaxRoots = 8;
		constexpr std::size_t kMaxObjects = 512;
		constexpr std::size_t kMaxGeometries = 64;
		constexpr unsigned kOpen = 1;
		constexpr unsigned kClose = 2;
		constexpr unsigned kSlider = 4;
		std::atomic<unsigned> g_events{ 0 };
		bool g_menuInstalled = false;
		bool g_sliderInstalled = false;
		bool g_haveSample = false;
		std::string g_lastSample;
		std::uint64_t g_sequence = 0;
		std::chrono::steady_clock::time_point g_lastSampleTime{};

		bool Enabled()
		{
			return spdlog::should_log(spdlog::level::debug);
		}

		class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
				RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event && a_event->menuName == "RaceSex Menu") {
					g_events.fetch_or(a_event->opening ? kOpen : kClose, std::memory_order_relaxed);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class SliderSink final : public RE::BSTEventSink<SKSE::ModCallbackEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
				RE::BSTEventSource<SKSE::ModCallbackEvent>*) override
			{
				if (a_event && a_event->eventName == "RSM_SliderChange") {
					g_events.fetch_or(kSlider, std::memory_order_relaxed);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		std::uintptr_t Identity(const void* a_pointer)
		{
			return reinterpret_cast<std::uintptr_t>(a_pointer);
		}

		template <class T, std::size_t N>
		bool Seen(const std::array<T*, N>& a_seen, std::size_t a_count, T* a_value)
		{
			for (std::size_t i = 0; i < a_count; ++i) {
				if (a_seen[i] == a_value) { return true; }
			}
			return false;
		}

		struct Scan
		{
			std::ostringstream text;
			std::array<RE::NiAVObject*, kMaxObjects> seen{};
			std::size_t objects = 0;
			std::size_t edges = 0;
			std::size_t geometries = 0;
			std::size_t hair = 0;
			bool truncated = false;

			void Visit(RE::NiAVObject* a_object, unsigned a_depth)
			{
				if (!a_object || Seen(seen, objects, a_object)) { return; }
				if (objects == kMaxObjects || a_depth >= 64) {
					truncated = true;
					return;
				}
				seen[objects++] = a_object;
				if (auto* geometry = a_object->AsGeometry()) {
					if (geometries == kMaxGeometries) { truncated = true; return; }
					++geometries;
					text << " geom=" << Identity(geometry) << " name=";
					const char* name = geometry->name.c_str();
					if (name) {
						for (std::size_t i = 0; i < 96 && name[i]; ++i) {
							const unsigned char c = static_cast<unsigned char>(name[i]);
							text << (c >= 32 && c != 127 ? name[i] : '?');
						}
					}
					auto* shader = geometry->GetGeometryRuntimeData().shaderProperty.get();
					text << " shader=" << Identity(shader);
					auto* lighting = shader ? netimmerse_cast<RE::BSLightingShaderProperty*>(shader) : nullptr;
					auto* material = lighting ? lighting->material : nullptr;
					text << " material=" << Identity(material);
					if (!material) { text << " skip=no-lighting-material;"; return; }
					const auto type = material->GetType();
					const auto feature = material->GetFeature();
					text << " type=" << static_cast<int>(type) << " feature=" << static_cast<int>(feature);
					if (type == RE::BSShaderMaterial::Type::kLighting && feature == RE::BSShaderMaterial::Feature::kHairTint) {
						const auto color = static_cast<const RE::BSLightingShaderMaterialHairTint*>(material)->tintColor;
						++hair;
						text << " tint=" << color.red << ',' << color.green << ',' << color.blue
							<< " finite=" << (std::isfinite(color.red) && std::isfinite(color.green) && std::isfinite(color.blue));
					}
					text << ';';
					return;
				}
				if (auto* node = a_object->AsNode()) {
					for (const auto& child : node->GetChildren()) {
						if (edges == kMaxObjects || objects == kMaxObjects || geometries == kMaxGeometries) {
							truncated = true;
							break;
						}
						++edges;
						Visit(child.get(), a_depth + 1);
					}
				}
			}
		};
	}

	void Install()
	{
		static MenuSink menuSink;
		static SliderSink sliderSink;
		if (!g_menuInstalled) {
			if (auto* ui = RE::UI::GetSingleton()) {
				ui->AddEventSink<RE::MenuOpenCloseEvent>(&menuSink);
				g_menuInstalled = true;
			}
		}
		if (!g_sliderInstalled) {
			if (auto* source = SKSE::GetModCallbackEventSource()) {
				source->AddEventSink(&sliderSink);
				g_sliderInstalled = true;
			}
		}
	}

	void Reset()
	{
		g_events.store(0, std::memory_order_relaxed);
		g_haveSample = false;
		g_lastSample.clear();
		g_sequence = 0;
		g_lastSampleTime = {};
	}

	void SampleOnMainThread(std::uint32_t a_coveredSlotMask)
	{
		if (!Enabled()) {
			g_events.exchange(0, std::memory_order_relaxed);
			g_haveSample = false;
			return;
		}
		const auto now = std::chrono::steady_clock::now();
		if (now - g_lastSampleTime < std::chrono::milliseconds(250)) {
			return;
		}
		g_lastSampleTime = now;
		const auto events = g_events.exchange(0, std::memory_order_relaxed);
		Scan scan;
		scan.text << std::setprecision(9);
		auto* ui = RE::UI::GetSingleton();
		auto* player = RE::PlayerCharacter::GetSingleton();
		scan.text << "menu=" << (ui ? (ui->IsMenuOpen("RaceSex Menu") ? "open" : "closed") : "unavailable")
			<< " mask=" << a_coveredSlotMask << " player=" << (player ? player->GetFormID() : 0);
		const auto* base = player ? player->GetActorBase() : nullptr;
		const auto* colorForm = base && base->headRelatedData ? base->headRelatedData->hairColor : nullptr;
		scan.text << " base=" << (base ? base->GetFormID() : 0) << " hairColorForm=" << (colorForm ? colorForm->GetFormID() : 0);
		if (colorForm) {
			const auto color = colorForm->color;
			scan.text << " baseRGBA=" << static_cast<unsigned>(color.red) << ',' << static_cast<unsigned>(color.green)
				<< ',' << static_cast<unsigned>(color.blue) << ',' << static_cast<unsigned>(color.alpha);
		} else {
			scan.text << " baseRGBA=missing";
		}
		const bool loaded = player && player->Is3DLoaded();
		scan.text << " loaded=" << loaded;
		std::array<RE::NiAVObject*, kMaxRoots> roots{};
		std::size_t rootCount = 0;
		if (loaded) {
			auto* headHair = player->GetHeadPartObject(RE::BGSHeadPart::HeadPartType::kHair);
			scan.text << " source=headpart-hair root=" << Identity(headHair);
			if (headHair) {
				roots[rootCount++] = headHair;
				scan.Visit(headHair, 0);
			} else {
				scan.text << " missing;";
			}
		}
		scan.text << " source=illusion-slots";
		if (loaded && a_coveredSlotMask) {
			for (const bool firstPerson : { false, true }) {
				const auto biped = player->GetBiped(firstPerson);
				scan.text << " view=" << (firstPerson ? "1p" : "3p") << " biped=" << Identity(biped.get());
				if (!biped) { continue; }
				for (std::uint32_t slot = 0; slot < 32; ++slot) {
					if (!(a_coveredSlotMask & (1u << slot))) { continue; }
					const auto& object = biped->objects[slot];
					auto* root = object.partClone.get();
					scan.text << " slot=" << slot << " item=" << (object.item ? object.item->GetFormID() : 0)
						<< " addon=" << (object.addon ? object.addon->GetFormID() : 0) << " root=" << Identity(root);
					if (!root) { scan.text << " missing;"; continue; }
					if (Seen(roots, rootCount, root)) { scan.text << " shared-root;"; continue; }
					if (rootCount == kMaxRoots) { scan.truncated = true; scan.text << " root-budget;"; continue; }
					roots[rootCount++] = root;
					scan.Visit(root, 0);
				}
			}
		}
		scan.text << " roots=" << rootCount << " objects=" << scan.objects << " edges=" << scan.edges
			<< " geometries=" << scan.geometries << " hair=" << scan.hair << " truncated=" << scan.truncated;
		auto sample = scan.text.str();
		if (!g_haveSample || events || sample != g_lastSample) {
			const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now().time_since_epoch()).count();
			SKSE::log::debug("HairTintDiagnostics: seq={} steadyMs={} events={} (open=1 close=2 slider=4; coalesced, not completion) {}",
				++g_sequence, milliseconds, events, sample);
		}
		g_lastSample = std::move(sample);
		g_haveSample = true;
	}
}
