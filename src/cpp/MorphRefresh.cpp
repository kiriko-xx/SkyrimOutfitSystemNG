#include "MorphRefresh.h"

#include "NGFeatures.h"
#include "bindings.h"

#include "core/Settings.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace SOS::MorphRefresh
{
	namespace
	{
		// Minimal ABI declarations for the RaceMenu interfaces we use. The
		// vtable order matters; it was audited against the public RaceMenu
		// header. The callback slot requires GetVersion() >= 5.
		namespace skee
		{
			using u32 = std::uint32_t;

			class IPluginInterface
			{
			public:
				virtual ~IPluginInterface() = default;
				virtual u32 GetVersion() = 0;
				virtual void Revert() = 0;
			};

			class IInterfaceMap
			{
			public:
				virtual IPluginInterface* QueryInterface(const char* a_name) = 0;
				virtual bool AddInterface(const char* a_name, IPluginInterface* a_interface) = 0;
				virtual IPluginInterface* RemoveInterface(const char* a_name) = 0;
			};

			struct InterfaceExchangeMessage
			{
				enum
				{
					kMessage_ExchangeInterface = 0x9E3779B9
				};

				IInterfaceMap* interfaceMap{ nullptr };
			};

			class IBodyMorphInterface : public IPluginInterface
			{
			public:
				class MorphKeyVisitor
				{
				public:
					virtual void Visit(const char*, float) = 0;
				};

				class MorphValueVisitor
				{
				public:
					virtual void Visit(void*, const char*, const char*, float) = 0;
				};

				// Slots 3..25 of the public header, in order. The types are
				// deliberately opaque here; this translation unit only needs the
				// slot order.
				virtual void SetMorph(void*, const char*, const char*, float) = 0;       // 3
				virtual float GetMorph(void*, const char*, const char*) = 0;            // 4
				virtual void ClearMorph(void*, const char*, const char*) = 0;           // 5
				virtual float GetBodyMorphs(void*, const char*) = 0;                 // 6
				virtual void ClearBodyMorphNames(void*, const char*) = 0;                            // 7
				virtual void VisitMorphs(void*, MorphKeyVisitor&) = 0;                   // 8
				virtual void VisitKeys(void*, const char*, MorphKeyVisitor&) = 0;                     // 9
				virtual void VisitMorphValues(void*, MorphValueVisitor&) = 0;            // 10
				virtual void ClearMorphs(void*) = 0;                                    // 11
				virtual void ApplyVertexDiff(void*, void*, bool) = 0;                   // 12
				virtual void ApplyBodyMorphs(void*, bool) = 0;                          // 13
				// 14..24 are never called. They exist so that the v5 entry point
				// below lands on the slot skee put it on: appending it right after
				// ApplyBodyMorphs would read ten slots past the end of the interface.
				virtual void UpdateModelWeight(void*, bool) = 0;                        // 14
				virtual void SetCacheLimit(std::uint64_t) = 0;                          // 15
				virtual bool HasMorphs(void*) = 0;                                      // 16
				virtual u32 EvaluateBodyMorphs(void*) = 0;                              // 17
				virtual bool HasBodyMorph(void*, const char*, const char*) = 0;         // 18
				virtual bool HasBodyMorphName(void*, const char*) = 0;                  // 19
				virtual bool HasBodyMorphKey(void*, const char*) = 0;                   // 20
				virtual void ClearBodyMorphKeys(void*, const char*) = 0;                // 21
				virtual void VisitStrings(void*) = 0;                                   // 22
				virtual void VisitActors(void*) = 0;                                    // 23
				virtual std::uint64_t ClearMorphCache() = 0;                            // 24

				// v5: "RaceMenu just wrote morphs into this shape". Fired per
				// shape, after the vertex buffer was rewritten and before the
				// upload. Last entry of the interface - on a v4 build calling it
				// reads past the end of the vtable, so it is gated on GetVersion().
				using MorphShapeCallback = void (*)(void*, void*, void*, void*, void*);
				virtual void AddMorphShapeCallback(MorphShapeCallback, std::uint64_t) = 0;  // 25
			};

			// kPluginVersion5 added AddMorphShapeCallback.
			inline constexpr u32 kBodyMorphVersionWithShapeCallback = 5;
		}

		// Minimal ABI declarations for the OBody plugin API. Only the one call
		// used here is named; the other slots keep the vtable order intact.
		namespace obody
		{
			enum class PluginAPIVersion : std::uint32_t
			{
				Invalid = 0,
				v1 = 1,
				Latest = v1
			};

			class IPluginInterface;
			class IOBodyReadinessEventListener;

			class IPluginInterfaceVersionIndependent
			{
			public:
				const char* owner{ nullptr };
				void* context{ nullptr };
				virtual PluginAPIVersion PluginAPIVersion() = 0;
				virtual const char* SetOwner(const char*) = 0;
			};

			struct PresetAssignmentInformation
			{
				enum Flags : std::uint64_t
				{
					None = 0,
					IsFemale = 1ull << 0
				};

				Flags flags{ None };
				std::string_view presetName;
			};

			class IPluginInterface : public IPluginInterfaceVersionIndependent
			{
			public:
				// Slots 2..17 of the public API. Only slot 17 is called; the
				// unused slots exist solely to preserve the vtable offsets.
				virtual bool ActorIsNaked(RE::Actor*) = 0;
				virtual bool ActorIsNaked(RE::Actor*, bool, const RE::TESForm*) = 0;
				virtual bool ActorHasORefitApplied(RE::Actor*) = 0;
				virtual bool ActorIsProcessed(RE::Actor*) = 0;
				virtual bool ActorIsBlacklisted(RE::Actor*) = 0;
				virtual bool IsORefitEnabled() = 0;
				virtual bool RegisterEventListener(void*) = 0;
				virtual bool DeregisterEventListener(void*) = 0;
				virtual bool HasRegisteredEventListener(void*) = 0;
				virtual void GetPresetCounts(void*) = 0;
				virtual std::size_t GetPresetNames(std::uint64_t, std::string_view*, std::size_t, std::size_t, std::size_t) = 0;
				virtual void EnsureActorIsProcessed(RE::Actor*) = 0;
				virtual void ApplyOBodyMorphsToActor(RE::Actor*) = 0;
				virtual void RemoveOBodyMorphsFromActor(RE::Actor*) = 0;
				virtual void ForcefullyChangeORefitForActor(RE::Actor*, bool) = 0;
				virtual void GetPresetAssignedToActor(RE::Actor*, PresetAssignmentInformation&) = 0;
			};

			class IOBodyReadinessEventListener
			{
			public:
				virtual void OBodyIsReady() = 0;
				virtual void OBodyIsNoLongerReady() = 0;
				virtual void OBodyIsBecomingReady() {}
				virtual void OBodyIsBecomingUnready() {}
			};

			struct RequestPluginInterface
			{
				static constexpr std::uint32_t type = 0xc0B0D9cc;
				PluginAPIVersion version{ PluginAPIVersion::v1 };
				IPluginInterface** pluginInterface{ nullptr };
				IOBodyReadinessEventListener* readinessEventListener{ nullptr };
			};

			static_assert(sizeof(RequestPluginInterface) == 24);
			static_assert(sizeof(PresetAssignmentInformation) == 24);
		}

		// Minimal ABI for SLIF NG's read-only query interface. SLIF NG (the
		// inflation framework, BodyInflationNG.dll) drives the actor's shape
		// through RaceMenu - BodySlide morphs and skeleton node scale - so its
		// applied values are a direct signal that the shape changed. Unlike the
		// skee and OBody handshakes this one is a plain C export, which makes it
		// independent of SKSE's receiver-name resolution. Only the called slots
		// are named; the others keep the vtable offsets intact.
		namespace slif
		{
			class IQueryInterface1
			{
			public:
				virtual std::uint32_t Version() const = 0;                                        // 0
				virtual bool IsTracked(RE::Actor*) const = 0;                                     // 1
				virtual std::uint32_t TrackedActorCount() const = 0;                              // 2
				virtual float GetValue(RE::Actor*, const char*, const char*, float) const = 0;    // 3
				virtual float GetMinValue(RE::Actor*, const char*, const char*, float) const = 0; // 4
				virtual float GetMaxValue(RE::Actor*, const char*, const char*, float) const = 0; // 5
				virtual float GetApplied(RE::Actor*, const char*) const = 0;                      // 6
				virtual float GetCombinedMorph(RE::Actor*, const char*) const = 0;                // 7
				virtual std::uint32_t GetCalculationType() const = 0;                             // 8
				virtual bool IsIncrementalInflation() const = 0;                                  // 9
			};

			struct InterfaceExchangeMessage
			{
				enum : std::uint32_t
				{
					kMessageType = 0x534C4946  // 'SLIF'
				};

				IQueryInterface1* query{ nullptr };
			};

			inline constexpr const char* kModuleName = "BodyInflationNG.dll";
			inline constexpr const char* kExportName = "SLIFNG_GetQueryInterface";

			using GetQueryInterfaceFn = IQueryInterface1* (*)();

			// The canonical skeleton-node inflation targets. This is the node
			// scale half of SLIF NG: 1.0 is neutral, so a mod that inflates
			// through SLIF_Morph (a BodySlide slider) leaves every one of them
			// untouched. The slider half is sampled separately, below.
			inline constexpr const char* kKeys[] = { "slif_belly", "slif_breast", "slif_butt", "slif_scrotum" };

			// The BodySlide sliders inflation mods write through SLIF_Morph
			// ("morph:<slider>", 0.0 neutral). These are the spellings the shipped
			// belly / breast / buttocks packs use; a slider that is not listed here
			// is still caught by the geometry probe, which needs no key at all.
			inline constexpr const char* kSliders[] = {
				"PregnancyBelly",
				"PregnancyBelly_Edge",
				"PregnancyBelly_SLIF",
				"BelatedBelly",
				"Breasts",
				"BreastsSH",
				"Breasts_SC",
				"Butt",
				"Butt_SC",
				"GenitalsScrotum"
			};
		}

		constexpr auto kPollInterval = std::chrono::milliseconds(250);
		constexpr auto kForceWindow = std::chrono::milliseconds(4000);
		constexpr auto kApplyDelay = std::chrono::milliseconds(150);
		// A morph or inflation provider usually writes several values in a row
		// (SLIF ramps an inflation in, OBody applies a preset step by step).
		// Rebuilding on every intermediate value would thrash the illusion, so a
		// changed signature only triggers a rebuild once it has sat still for this
		// long. The geometry probe needs a little more than the provider values:
		// the morphed vertex buffer is rewritten a frame or two after the morph.
		constexpr auto kSignatureSettle = std::chrono::milliseconds(350);
		// Floor between two illusion cycles of the same actor. A cycle is two
		// full refreshes, so this doubles as the anti-stutter guard while a
		// slider is being dragged: 1.2 s keeps it fluid and never stacks cycles.
		constexpr auto kMinCycleInterval = std::chrono::milliseconds(1200);
		// After a cycle the shape keeps moving for a while on its own: the new
		// clones receive the morph in the delayed apply pass, and OBody re-derives
		// its preset because our rebuild looks like an equipment change. Without
		// a quiet window every one of those would count as a fresh morph change
		// and start the next cycle - a slider drag would never settle. Inside the
		// window a change only re-baselines the signature, never fires.
		constexpr auto kPostCycleQuiet = std::chrono::milliseconds(2500);
		// RaceMenu can answer the interface exchange late on 1.5.97; ask again
		// at this interval until RaceMenu responded or the attempts ran out.
		constexpr auto kProviderRetryInterval = std::chrono::milliseconds(5000);
// Retry cadence: fast for the first eight tries (the load sequence), then one
// attempt every 30 s for as long as the plugin runs. 1.5.18 stopped after
// 12 x 5 s, a budget that expired during the main menu - before 1.5.97 RaceMenu
// has registered its message listener at all - so the interface was never
// acquired and every morph feature silently fell back to the geometry probe.
constexpr auto kProviderRetrySlowInterval = std::chrono::milliseconds(30000);
// Lazy retry: asked again right before the interface is actually needed.
constexpr auto kUseRetryInterval = std::chrono::milliseconds(2500);
// How long the provider pass gets to move the meshes before the fallback cycle
// runs instead.
constexpr auto kVerifyDelay = std::chrono::milliseconds(350);
// How many additional 350 ms windows the provider pass gets before the cycle
// fallback runs (4 x 350 ms ~= 1.4 s, enough for a deferred skee flush).
		constexpr int kVerifyAttempts = 4;
		// A change that lands this long after one of our own passes is not the
		// tail of that pass any more: our rebuild settles within a frame or two,
		// so anything later is a genuine morph change that merely arrived while
		// the quiet window was still open. 1.5.19 absorbed those as a new
		// baseline, so a slider drag that finished inside the window never
		// refreshed the illusion at all. They are now replayed once the window
		// closes.
		constexpr auto kSelfNoiseGrace = std::chrono::milliseconds(1200);
		// Upper bound on the extra passes a quiet window may chain. OBody
		// re-derives its preset after one of our passes, which reads exactly like
		// a fresh morph change; without a bound that ping-pong never ends.
		constexpr int kMaxQuietPasses = 3;

		std::mutex g_mutex;
		skee::IBodyMorphInterface* g_api{ nullptr };
		// Version the BodyMorph interface reported; gates the v5 callback.
		std::uint32_t g_apiVersion{ 0 };
		obody::IPluginInterface* g_obody{ nullptr };
		slif::IQueryInterface1* g_slif{ nullptr };
		std::atomic<bool> g_obodyReady{ false };
		bool g_installed{ false };
		bool g_pollPending{ false };
		std::chrono::steady_clock::time_point g_lastPoll{};
		std::chrono::steady_clock::time_point g_lastProviderRetry{};
		int g_providerRetries{ 0 };

		// One queued provider retry at a time. The handshake dispatches SKSE
		// messages, so it may only run from a main-thread task - never from an
		// SKSE message handler and never from a skinning hook.
		std::atomic<bool> g_providerRetryQueued{ false };

		void QueueProviderRetry()
		{
			if (g_providerRetryQueued.exchange(true)) {
				return;
			}
			auto* tasks = SKSE::GetTaskInterface();
			if (!tasks) {
				g_providerRetryQueued.store(false);
				return;
			}
			tasks->AddTask([]() {
				g_providerRetryQueued.store(false);
				RetryProviders();
			});
		}

		// Per actor: the signature last observed, the stabilised signature that
		// already caused a pass, the timestamps both need, and what the provider
		// showed when that pass started - a morph value that moves away from it
		// later is a new change even if it arrives inside the quiet window.
		struct SignatureState
		{
			std::uint64_t value{ 0 };
			std::uint64_t fired{ 0 };
			std::uint64_t actedProvider{ 0 };
			std::chrono::steady_clock::time_point valueSince{};
			std::chrono::steady_clock::time_point lastCycle{};
			std::chrono::steady_clock::time_point quietUntil{};
			std::chrono::steady_clock::time_point quietFrom{};
			bool quietDrift{ false };
			bool verifyActive{ false };
			int quietPasses{ 0 };
		};

		std::unordered_map<RE::FormID, SignatureState> g_signatures;
		std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> g_forced;
		std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> g_pendingApply;
		std::unordered_set<RE::FormID> g_applyScheduled;

		// Provider-first verification: the covered-slot geometry recorded when the
		// provider pass was queued, and when it may be checked.
		struct VerifyState
		{
			std::uint64_t before{ 0 };
			std::chrono::steady_clock::time_point when{};
			int attempts{ 0 };
		};
		std::unordered_map<RE::FormID, VerifyState> g_verify;

		struct PendingReillusion
		{
			std::string name;
			int phase{ 0 };
			std::chrono::steady_clock::time_point due{};
		};

		std::unordered_map<RE::FormID, PendingReillusion> g_reillusion;

		class OBodyReadinessListener final : public obody::IOBodyReadinessEventListener
		{
		public:
			void OBodyIsReady() override
			{
				g_obodyReady.store(true, std::memory_order_release);
				SKSE::log::info("MorphRefresh: OBody is ready; preset changes will rebuild the illusion.");
			}

			void OBodyIsNoLongerReady() override
			{
				g_obodyReady.store(false, std::memory_order_release);
				SKSE::log::info("MorphRefresh: OBody is no longer ready; preset polling is paused.");
			}
		};

		OBodyReadinessListener g_obodyReadiness;

		class HashVisitor final : public skee::IBodyMorphInterface::MorphValueVisitor
		{
		public:
			std::uint64_t hash{ 0 };

			void Visit(void*, const char* a_name, const char* a_key, float a_value) override
			{
				// Order-independent: the visitor may walk an unordered map.
				std::uint64_t h = 1469598103934665603ull;
				auto mix = [&h](const char* a_text) {
					if (!a_text) {
						return;
					}
					while (*a_text) {
						h ^= static_cast<unsigned char>(*a_text++);
						h *= 1099511628211ull;
					}
				};
				mix(a_name);
				mix(a_key);
				std::uint32_t bits{ 0 };
				static_assert(sizeof(bits) == sizeof(a_value));
				std::memcpy(&bits, &a_value, sizeof(bits));
				h ^= bits;
				h *= 1099511628211ull;
				hash += h;
			}
		};

		std::uint64_t HashText(std::string_view a_text)
		{
			std::uint64_t hash = 1469598103934665603ull;
			for (const char ch : a_text) {
				hash ^= static_cast<unsigned char>(ch);
				hash *= 1099511628211ull;
			}
			hash ^= a_text.size();
			hash *= 1099511628211ull;
			return hash;
		}

		// Hash the vertex data of one attached shape. Only dynamic tri shapes carry
		// morphs (.tri sliders), and every body morph provider ends up writing the
		// morphed positions into their dynamic buffer - RaceMenu sliders, OBody
		// presets and SLIF inflation alike, whichever API they use to get there.
		// That makes the buffer itself a provider independent "the shape changed"
		// signal, which is what 1.5.18 needed: on 1.5.97 RaceMenu never answers the
		// interface exchange, so the morph values themselves stay invisible to us.
		std::uint64_t HashDynamicShape(RE::NiAVObject* a_object)
		{
			if (!a_object) {
				return 0;
			}
			auto* shape = a_object->AsDynamicTriShape();
			if (!shape) {
				return 0;
			}
			const auto& runtime = shape->GetDynamicTrishapeRuntimeData();
			const std::size_t size = runtime.dataSize;
			if (!runtime.dynamicData || size < 64) {
				return 0;
			}
			const auto* bytes = static_cast<const std::uint8_t*>(runtime.dynamicData);
			std::uint64_t hash = 1469598103934665603ull;
			// The belly / breast morphs sculpt the first vertices, so the head of
			// the buffer is walked byte by byte.
			const std::size_t head = size < 1024 ? size : 1024;
			for (std::size_t i = 0; i < head; ++i) {
				hash ^= bytes[i];
				hash *= 1099511628211ull;
			}
			// The rest with a prime stride: a morph moves whole regions, so a
			// sparse walk still notices every edit and stays cheap at 250 ms.
			for (std::size_t i = head; i < size; i += 251) {
				hash ^= bytes[i];
				hash *= 1099511628211ull;
			}
			hash ^= size;
			hash *= 1099511628211ull;
			return hash;
		}

		// 1.5.26: a dressed actor's body and armour are *skinned* meshes -
		// BSTriShape with a NiSkinInstance - so HashDynamicShape() above never
		// touches them and the poll stayed blind while a slider was dragged. The
		// morphed positions of a skinned mesh live in the partition's raw vertex
		// buffer (that is exactly where skee writes them, see
		// BodyMorphInterface.cpp ApplyMorph), and those vertices are stored in
		// bind pose, so animation alone never changes this hash.
		std::uint64_t HashSkinnedShape(RE::NiAVObject* a_object)
		{
			if (!a_object) {
				return 0;
			}
			auto* geometry = a_object->AsGeometry();
			if (!geometry || !geometry->skinInstance) {
				return 0;
			}
			auto* partition = geometry->skinInstance->skinPartition.get();
			if (!partition || partition->numPartitions == 0 || !partition->partitions.data()) {
				return 0;
			}
			auto& part = partition->partitions[0];
			auto* buffer = part.buffData;
			if (!buffer || !buffer->rawVertexData || part.vertices == 0) {
				return 0;
			}
			const std::uint32_t stride = part.vertexDesc.GetSize();
			if (stride == 0 || stride > 128) {
				return 0;
			}
			const std::size_t size = static_cast<std::size_t>(part.vertices) * stride;
			const auto* bytes = buffer->rawVertexData;
			std::uint64_t hash = 1469598103934665603ull;
			const std::size_t head = size < 1024 ? size : 1024;
			for (std::size_t i = 0; i < head; ++i) {
				hash ^= bytes[i];
				hash *= 1099511628211ull;
			}
			for (std::size_t i = head; i < size; i += 251) {
				hash ^= bytes[i];
				hash *= 1099511628211ull;
			}
			hash ^= size;
			hash *= 1099511628211ull;
			return hash;
		}

		// 1.5.18 hashed biped->objects[i].partClone directly and stayed blind: for
		// most pieces that pointer is the *container* of the piece (a NiNode that
		// holds the mesh, an add-on node, a skin wrapper), not the BSGeometry, so
		// AsDynamicTriShape() failed on every slot and the probe never fired - the
		// "the illusion does not follow the slider while dressed" symptom. Walk the
		// subtree instead, with a depth and shape budget so a 250 ms poll stays
		// cheap even on a modded actor. The budget is per slot: a dense body mesh
		// used to eat the whole allowance and the armour slots the illusion lives
		// in were never sampled at all.
		constexpr int kProbeMaxDepth = 6;
		constexpr std::size_t kProbeShapesPerSlot = 8;
		constexpr std::size_t kProbeShapesTotal = 96;

		void HashSubtree(RE::NiAVObject* a_object, int a_depth, std::size_t a_budget, std::uint64_t& a_hash, std::size_t& a_shapes)
		{
			if (!a_object || a_depth > kProbeMaxDepth || a_shapes >= a_budget) {
				return;
			}
			auto shape = HashDynamicShape(a_object);
			if (shape == 0) {
				shape = HashSkinnedShape(a_object);
			}
			if (shape != 0) {
				a_hash ^= shape;
				a_hash *= 1099511628211ull;
				++a_shapes;
			}
			auto* node = a_object->AsNode();
			if (!node) {
				return;
			}
			for (auto& child : node->GetChildren()) {
				HashSubtree(child.get(), a_depth + 1, a_budget, a_hash, a_shapes);
			}
		}

		// a_mask selects biped slots (bit i == objects[i], the same bit order the
		// armour BOD2 mask uses); 0 means every slot.
		std::uint64_t ComputeGeometrySignature(RE::Actor* a_actor, std::uint32_t a_mask = 0)
		{
			if (!a_actor) {
				return 0;
			}
			std::uint64_t hash = 1469598103934665603ull;
			std::size_t shapes{ 0 };
			for (const bool firstPerson : { false, true }) {
				auto* biped = a_actor->GetBiped(firstPerson).get();
				if (!biped) {
					continue;
				}
				for (std::size_t i = 0; i < RE::BIPED_OBJECTS::kTotal; ++i) {
					if (shapes >= kProbeShapesTotal) {
						break;
					}
					if (a_mask != 0 && !(i < 32 && (a_mask & (1u << i)) != 0)) {
						// A mask means "only these slots": anything it cannot
						// name (beyond bit 31) is out of scope for the check.
						continue;
					}
					std::uint64_t slot = 1469598103934665603ull;
					std::size_t slotShapes{ 0 };
					HashSubtree(biped->objects[i].partClone.get(), 0, kProbeShapesPerSlot, slot, slotShapes);
					if (slotShapes == 0) {
						continue;
					}
					shapes += slotShapes;
					// Position sensitive: the same mesh on another slot is a
					// different contribution.
					hash ^= slot + 0x9E3779B97F4A7C15ull * (i + 1);
					hash *= 1099511628211ull;
				}
			}
			return shapes > 0 ? hash : 0;
		}

		// The slots the current illusion actually covers. Verifying a provider
		// pass against these - and not against the whole actor - is what makes the
		// check honest: while a slider is dragged the body mesh underneath is
		// rewritten too, so a whole-actor signature always "moves", 1.5.19 then
		// skipped the fallback cycle (the only path that reloads a .tri target)
		// and the dressed illusion stayed stale.
		std::uint32_t CoveredSlotMask(RE::FormID a_id)
		{
			std::uint32_t mask = 0;
			auto service = outfit_service_get_singleton_ptr();
			const auto* outfit = service->inner().current_outfit_ptr(a_id);
			if (!outfit) {
				return 0;
			}
			for (const auto* armor : outfit->armors()) {
				if (armor) {
					mask |= armor->GetSlotMask().underlying();
				}
			}
			return mask;
		}

		std::uint64_t ComputeCoveredGeometry(RE::Actor* a_actor, RE::FormID a_id)
		{
			return ComputeGeometrySignature(a_actor, CoveredSlotMask(a_id));
		}

		std::uint64_t ComputeOBodySignature(RE::Actor* a_actor)
		{
			if (!g_obody || !g_obodyReady.load(std::memory_order_acquire) || !a_actor) {
				return 0;
			}
			obody::PresetAssignmentInformation info{};
			info.flags = obody::PresetAssignmentInformation::None;
			g_obody->GetPresetAssignedToActor(a_actor, info);
			return HashText(info.presetName);
		}

		std::uint64_t ComputeSLIFSignature(RE::Actor* a_actor)
		{
			if (!g_slif || !a_actor || !g_slif->IsTracked(a_actor)) {
				return 0;
			}
			std::uint64_t hash = 1469598103934665603ull;
			auto mix = [&hash](float a_value) {
				std::uint32_t bits{ 0 };
				static_assert(sizeof(bits) == sizeof(a_value));
				std::memcpy(&bits, &a_value, sizeof(bits));
				hash ^= bits;
				hash *= 1099511628211ull;
			};
			for (const auto* key : slif::kKeys) {
				mix(g_slif->GetApplied(a_actor, key));
			}
			// The slider half. GetCombinedMorph is the cross-mod total of one
			// BodySlide slider, GetApplied("morph:<slider>") the value SLIF NG
			// currently shows for it; reading both means a ramped, held or
			// released inflation all move the signature. This is the case 1.5.17
			// missed: Fill Her Up inflates through SLIF_Morph, so every node key
			// stayed at its 1.0 neutral and the illusion was never rebuilt.
			for (const auto* slider : slif::kSliders) {
				mix(g_slif->GetCombinedMorph(a_actor, slider));
				char target[64]{};
				std::snprintf(target, sizeof(target), "morph:%s", slider);
				mix(g_slif->GetApplied(a_actor, target));
			}
			return hash;
		}

		// Provider half of the signature: the morph values themselves. This is the
		// half our own rebuild cannot move - re-applying stored morphs does not
		// change them - which is what lets the quiet window tell "a change arrived
		// after our pass" from "our pass is still settling".
		std::uint64_t ComputeProviderSignature(RE::Actor* a_actor, bool* a_any = nullptr)
		{
			if (a_any) {
				*a_any = false;
			}
			if (!a_actor) {
				return 0;
			}
			bool any = false;
			std::uint64_t signature = 1469598103934665603ull;
			if (g_api) {
				HashVisitor visitor;
				g_api->VisitMorphValues(a_actor, visitor);
				signature ^= visitor.hash;
				signature *= 1099511628211ull;
				any = true;
			}
			if (g_obody && g_obodyReady.load(std::memory_order_acquire)) {
				signature ^= ComputeOBodySignature(a_actor);
				signature *= 1099511628211ull;
				any = true;
			}
			if (g_slif) {
				signature ^= ComputeSLIFSignature(a_actor);
				signature *= 1099511628211ull;
				any = true;
			}
			if (a_any) {
				*a_any = any;
			}
			return any ? signature : 0;
		}

		bool ProviderAvailable()
		{
			return g_api != nullptr || g_slif != nullptr ||
				(g_obody != nullptr && g_obodyReady.load(std::memory_order_acquire));
		}

		std::uint64_t ComputeSignature(RE::Actor* a_actor)
		{
			if (!a_actor) {
				return 0;
			}
			bool any = false;
			std::uint64_t signature = ComputeProviderSignature(a_actor, &any);
			if (!any) {
				signature = 1469598103934665603ull;
			}
			// Provider independent last resort: the morphed vertex buffers. This
			// is what makes the refresh work on a RaceMenu that never hands out
			// its interface, and for inflation sliders we do not know by name.
			if (const auto geometry = ComputeGeometrySignature(a_actor)) {
				signature ^= geometry;
				signature *= 1099511628211ull;
				any = true;
			}
			return any ? signature : 0;
		}

		void ApplyMorphs(RE::Actor* a_actor)
		{
			if (!g_api || !a_actor || !a_actor->Is3DLoaded()) {
				return;
			}
			g_api->ApplyBodyMorphs(a_actor, true);
			SKSE::log::trace("MorphRefresh: ApplyBodyMorphs for {:08X}.", a_actor->GetFormID());
		}

		void RestoreIllusion(RE::Actor* a_actor, RE::FormID a_id, const std::string& a_name)
		{
			{
				auto service = outfit_service_get_mut_singleton_ptr();
				service->inner().set_outfit_c(a_name.c_str(), a_id);
			}
			{
				std::lock_guard lock(g_mutex);
				g_forced[a_id] = std::chrono::steady_clock::now() + kForceWindow;
			}
			Features::RefreshActor(a_actor);
			SKSE::log::debug("MorphRefresh: illusion of {:08X} restored with a fresh morph target.", a_id);
		}

		bool CycleIllusion(RE::Actor* a_actor, const char* a_reason)
		{
			if (!a_actor || !a_actor->Is3DLoaded()) {
				return false;
			}

			const auto id = a_actor->GetFormID();
			std::string name;
			{
				auto service = outfit_service_get_singleton_ptr();
				if (const auto* outfit = service->inner().current_outfit_ptr(id)) {
					name = outfit->name_c().c_str();
				}
			}
			if (name.empty()) {
				return false;
			}

			{
				std::lock_guard lock(g_mutex);
				if (g_reillusion.contains(id)) {
					// A split cycle is still waiting for its second half.
					return true;
				}
			}

			SKSE::log::debug("MorphRefresh: taking the illusion of {:08X} off for one pass ({}).", id, a_reason);
			{
				auto service = outfit_service_get_mut_singleton_ptr();
				service->inner().set_outfit_c("", id);
			}
			{
				std::lock_guard lock(g_mutex);
				g_forced[id] = std::chrono::steady_clock::now() + kForceWindow;
			}
			// Pass one: no illusion override, so the engine applies the real armour
			// itself. That pass is what finally hands every display slot a freshly
			// created clone, which is the only thing that loads a new .tri target.
			Features::RefreshActor(a_actor);

			// Both refreshes normally run inside this very task, so no frame is drawn
			// with the real gear visible: the swap looks exactly like picking another
			// outfit in the quick picker. [Compatibility] ReillusionGapMs > 0 splits
			// the two passes for setups that need a frame in between.
			const auto gap = SOS::Settings::Get().reillusionGapMs;
			if (gap > 0) {
				std::lock_guard lock(g_mutex);
				auto& entry = g_reillusion[id];
				entry.name = name;
				entry.phase = 1;
				entry.due = std::chrono::steady_clock::now() + std::chrono::milliseconds(gap);
				return true;
			}

			RestoreIllusion(a_actor, id, name);
			return true;
		}

		bool ProviderCanMorph()
		{
			return g_api != nullptr;
		}

		void EnsureProvidersForUse()
		{
			// Ask again right before the interface is actually needed - never only
			// on a timer that may already have expired before the game started.
			// Always through the queue: this runs inside the skinning hook, and a
			// message dispatch must not happen there (1.5.25 logs show the
			// "Failed to dispatch message" warnings landing between two skinning
			// calls for exactly that reason).
			const auto now = std::chrono::steady_clock::now();
			{
				std::lock_guard lock(g_mutex);
				if (now - g_lastProviderRetry < kUseRetryInterval) {
					return;
				}
				g_lastProviderRetry = now;
			}
			QueueProviderRetry();
		}

		// What asked for a re-morph; only used for the log line.
		enum class BakeReason
		{
			kProvider,
			kRebuilt
		};

		// ------------------------------------------------------------------
		// Morph push - the "keep responding" half of the fix
		//
		// Everything above reacts after the fact: the 250 ms poll notices that a
		// provider wrote new values and only then re-drives the illusion. That is a
		// safety net, not a fix. For a RaceMenu slider being dragged two things are
		// missing, and both of them are invisible from the poll:
		//
		//  1. The illusion clone carries no .tri morph target at all. skee morphs a
		//     node only when it carries a "BODYTRI" extra data naming its .tri
		//     (MorphCache::ApplyMorphs looks for that name in the subtree it is
		//     handed and skips the whole subtree when it is absent). The clones the
		//     outfit hook builds with ApplyArmorAddon never pick that extra data up
		//     unless the .nif shipped with it, so the shape of the illusion cannot
		//     move, whatever the slider says. AttachBodyTri() puts the extra data on
		//     the clone, using the .tri that sits next to the armour model.
		//
		//  2. Nothing says "these vertices were just rewritten". skee v5 hands out
		//     MorphShapeCallback for exactly that, and OBody broadcasts
		//     "Obody_ApplyMorph" through SKSE's mod callback event. Both of them only
		//     mark the actor dirty here; the re-morph itself runs from a main-thread
		//     task - never inside the callback, because MorphCache::ApplyMorphs holds
		//     its lock while the callbacks fire and calling back into skee from there
		//     would deadlock the game.
		// ------------------------------------------------------------------
		namespace morphPush
		{
			std::mutex g_queueMutex;
			std::unordered_set<RE::FormID> g_pending;
			BakeReason g_reason{ BakeReason::kProvider };
			std::atomic<bool> g_taskQueued{ false };
			// Set while this module is the one writing vertices, so the skee callback
			// that fires for our own pass does not come back as "the illusion still
			// needs a pass" - that would ping-pong for as long as the game runs.
			std::atomic<bool> g_selfWrite{ false };
			bool g_shapeCallbackInstalled{ false };
			bool g_obodySinkInstalled{ false };
			// Floor between two passes, not per actor: a slider fires one callback per
			// morphed shape per frame and every pass walks all covered slots. 15 ms
			// caps the cost at roughly one pass per frame, which is invisible.
			constexpr auto kMinBakeGap = std::chrono::milliseconds(15);
			std::chrono::steady_clock::time_point g_lastBake{};
			// Evidence logging: one "did the vertices actually move" line per actor
			// per 5 s instead of one per frame while a slider is dragged.
			std::chrono::steady_clock::time_point g_lastEvidence{};
			RE::FormID g_evidenceActor{ 0 };
			// One info line per actor the first time its clones carry a .tri
			// target, so a user log proves the push path is armed without
			// turning every rebuild into a log line.
			std::unordered_set<RE::FormID> g_targetLogged;
			// Per actor: did the last pass leave .tri targets on its clones? When it
			// did, the illusion follows the next morph pass on its own and the cycle
			// (remove + reapply, the one thing that shows the real gear for a frame)
			// has nothing left to fix.
			std::unordered_map<RE::FormID, bool> g_armed;
		}

		enum class BakeResult
		{
			kSkipped,
			kRateLimited,
			kBaked
		};

		const char* BakeReasonText(BakeReason a_reason)
		{
			return a_reason == BakeReason::kRebuilt ?
			           "the illusion was rebuilt" :
			           "the provider wrote new morphs";
		}

		std::string MeshRelativePath(std::string a_path)
		{
			std::replace(a_path.begin(), a_path.end(), '/', '\\');
			std::transform(a_path.begin(), a_path.end(), a_path.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			while (a_path.starts_with("meshes\\")) {
				a_path.erase(0, 7);
			}
			if (a_path.empty() || a_path.front() == '\\' || a_path.find(':') != std::string::npos ||
				a_path == ".." || a_path.starts_with("..\\") || a_path.find("\\..") != std::string::npos) {
				return {};
			}
			return a_path;
		}

		bool TriFileExists(const std::string& a_relative)
		{
			// RaceMenu opens the file the same way (BodyMorphInterface::CacheFile,
			// BSResourceNiBinaryStream over "meshes\\" + path), so this answers
			// exactly the question RaceMenu will ask later.
			RE::BSResourceNiBinaryStream stream{ std::string{ "meshes\\" } + a_relative };
			return stream.good();
		}

		// Every .tri name a pack can ship for one mesh, most likely first. OBody
		// derives exactly one name (drop the ".nif" and a "_0"/"_1" sex suffix, add
		// ".tri"); real sets also ship the suffixed name, the other sex suffix or a
		// name that matches the .nif one to one, and ruling each of them out costs
		// one BSResource lookup.
		void TriCandidates(const char* a_model, bool a_female, std::vector<std::string>& a_out)
		{
			if (!a_model || !*a_model) {
				return;
			}
			const std::string model = MeshRelativePath(a_model);
			std::string lower{ model };
			std::transform(lower.begin(), lower.end(), lower.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (lower.size() <= 4 || lower.compare(lower.size() - 4, 4, ".nif") != 0) {
				return;
			}
			const std::string stem = model.substr(0, model.size() - 4);
			std::string trimmed = stem;
			bool suffixed = false;
			if (stem.size() > 2) {
				const auto tail = stem.substr(stem.size() - 2);
				if (tail == "_0" || tail == "_1") {
					trimmed = stem.substr(0, stem.size() - 2);
					suffixed = true;
				}
			}
			// SEXES: 0 = male, 1 = female.
			const char* sameSex = a_female ? "_1.tri" : "_0.tri";
			const char* otherSex = a_female ? "_0.tri" : "_1.tri";
			a_out.push_back(trimmed + ".tri");  // femalebody_1.nif -> femalebody.tri
			if (suffixed) {
				a_out.push_back(stem + ".tri");  // femalebody_1.nif -> femalebody_1.tri
			}
			a_out.push_back(trimmed + sameSex);
			a_out.push_back(trimmed + otherSex);
		}

		std::string CandidateListText(const char* a_model, bool a_female)
		{
			std::vector<std::string> candidates;
			TriCandidates(a_model, a_female, candidates);
			if (candidates.empty()) {
				return "none (no usable .nif model path)";
			}
			std::string text;
			for (const auto& candidate : candidates) {
				if (!text.empty()) {
					text += ", ";
				}
				text += candidate;
			}
			return text;
		}

		std::string DeriveTriPath(const char* a_model, bool a_female)
		{
			std::vector<std::string> candidates;
			TriCandidates(a_model, a_female, candidates);
			for (const auto& candidate : candidates) {
				if (TriFileExists(candidate)) {
					return candidate;
				}
			}
			return {};
		}

		// Main thread only. Keyed by ARMA, sex and perspective; an empty value means "no
		// .tri", which is the common case and must not be re-probed per rebuild.
		std::unordered_map<std::uint64_t, std::string> g_triPathCache;
		std::unordered_set<std::uint64_t> g_triProbeLogged;

		// Prefer the actual attached ARMA; a race lookup is only a fallback.
		std::string BodyTriPathFor(RE::TESObjectARMO* a_armor, RE::TESObjectARMA* a_addon, RE::TESRace* a_race, bool a_female, bool a_firstPerson)
		{
			if (!a_armor) {
				return {};
			}
			const RE::TESObjectARMA* addon = a_addon;
			const char* source = "the armour addon the engine attached";
			if (!addon && a_race) {
				addon = a_armor->GetArmorAddon(a_race);
				source = "an armour addon looked up for the race";
			}
			if (!addon) {
				// No cache entry: the lookup can start answering once the armour is
				// actually worn, and an empty value cached now would lock the slot out
				// of .tri targets for the rest of the session.
				SKSE::log::debug("MorphRefresh: {:08X} has no armour addon yet; no .tri target can be derived.", a_armor->GetFormID());
				return {};
			}
			const std::uint64_t key = (static_cast<std::uint64_t>(addon->GetFormID()) << 2) |
				(a_female ? 1ull : 0ull) | (a_firstPerson ? 2ull : 0ull);
			if (const auto it = g_triPathCache.find(key); it != g_triPathCache.end()) {
				return it->second;
			}
			const auto* models = a_firstPerson ? addon->bipedModel1stPersons : addon->bipedModels;
			const char* model = models[a_female ? 1 : 0].GetModel();
			if (!model || !*model) {
				model = models[a_female ? 0 : 1].GetModel();
			}
			const auto path = DeriveTriPath(model, a_female);
			if (g_triProbeLogged.insert(key).second) {
				if (path.empty()) {
					SKSE::log::debug("MorphRefresh: {:08X} slot model {} (from {}) has no .tri beside it, so RaceMenu cannot morph that slot. Probed: {}.",
						a_armor->GetFormID(), model && *model ? model : "<no model>", source,
						CandidateListText(model, a_female));
				} else {
					SKSE::log::debug("MorphRefresh: {:08X} slot model {} (from {}) uses .tri {}.",
						a_armor->GetFormID(), model && *model ? model : "<no model>", source, path);
				}
			}
			g_triPathCache.emplace(key, path);
			return path;
		}

		// RaceMenu looks for the extra data while walking the subtree it was handed,
		// and a cloned piece may already carry one (a .tri that shipped next to the
		// mesh, added by the engine). Never stack a second one.
		bool NodeHasBodyTri(RE::NiAVObject* a_node, int a_depth = 0)
		{
			if (!a_node || a_depth > kProbeMaxDepth) {
				return false;
			}
			for (std::uint16_t i = 0; i < a_node->GetExtraDataSize(); ++i) {
				auto* extra = a_node->GetExtraDataAt(i);
				if (!extra || std::strcmp(extra->GetName().c_str(), "BODYTRI") != 0) { continue; }
				auto* text = netimmerse_cast<RE::NiStringExtraData*>(extra);
				if (!text || !text->value || !*text->value) { continue; }
				const auto path = MeshRelativePath(text->value);
				if (path.empty() || !TriFileExists(path)) { continue; }
				if (path != text->value) {
					auto* value = RE::malloc<char>(path.size() + 1);
					if (!value) { continue; }
					std::memcpy(value, path.c_str(), path.size() + 1);
					RE::NiPointer<RE::NiStringExtraData> replacement{ RE::NiExtraData::Create<RE::NiStringExtraData>() };
					if (!replacement) { RE::free(value); continue; }
					replacement->SetName("BODYTRI");
					replacement->value = value;
					// Extra data may be shared with another clone; replace this node
					// rather than freeing or rewriting the shared string.
					RE::NiPointer<RE::NiExtraData> original{ extra };
					a_node->RemoveExtraData("BODYTRI");
					if (!a_node->AddExtraData(replacement.get())) {
						a_node->AddExtraData(original.get());
						continue;
					}
				}
				return true;
			}
			const auto* node = a_node->AsNode();
			if (!node) {
				return false;
			}
			for (const auto& child : node->GetChildren()) {
				if (NodeHasBodyTri(child.get(), a_depth + 1)) {
					return true;
				}
			}
			return false;
		}

		// The value RaceMenu reads is the path relative to "meshes\" (it prepends
		// the directory and lowercases the whole thing, CreateTRIPath). The string
		// has to come from the engine's heap: this plugin is linked against the
		// static CRT, so new[] here would be freed by a different allocator.
		bool AttachBodyTri(RE::NiAVObject* a_node, const std::string& a_path)
		{
			if (!a_node || a_path.empty() || NodeHasBodyTri(a_node)) {
				return false;
			}
			RE::NiPointer<RE::NiStringExtraData> extra{ RE::NiExtraData::Create<RE::NiStringExtraData>() };
			if (!extra) {
				return false;
			}
			extra->SetName("BODYTRI");
			auto* value = RE::malloc<char>(a_path.size() + 1);
			if (!value) {
				return false;
			}
			std::memcpy(value, a_path.c_str(), a_path.size() + 1);
			extra->value = value;
			RE::NiPointer<RE::NiExtraData> original{ a_node->GetExtraData("BODYTRI") };
			if (original) { a_node->RemoveExtraData("BODYTRI"); }
			if (a_node->AddExtraData(extra.get())) { return true; }
			if (original) { a_node->AddExtraData(original.get()); }
			return false;
		}

		// The poll below watches the same vertex buffers this pass just rewrote.
		// Without this the next poll would read our own write as "the provider
		// changed" and run a cycle - the visible flash this module exists to
		// remove. Adopt the result as the baseline and open the usual quiet
		// window; a genuine change that arrives inside it is still replayed by
		// the drift logic in PollOnMainThread().
		void AbsorbOwnWrite(RE::Actor* a_actor)
		{
			if (!a_actor) {
				return;
			}
			const auto id = a_actor->GetFormID();
			const auto baseline = ComputeSignature(a_actor);
			const auto providerSignature = ComputeProviderSignature(a_actor, nullptr);
			const auto now = std::chrono::steady_clock::now();
			std::lock_guard lock(g_mutex);
			// A check scheduled by the earlier provider pass would compare against
			// vertices this pass just moved and call it "the provider did nothing".
			g_verify.erase(id);
			auto [it, inserted] = g_signatures.try_emplace(id);
			auto& state = it->second;
			state.value = baseline;
			state.fired = baseline;
			state.actedProvider = providerSignature;
			state.valueSince = now;
			state.quietUntil = now + kSelfNoiseGrace;
			state.quietFrom = now;
			state.quietDrift = false;
			state.verifyActive = false;
			state.quietPasses = 0;
			if (inserted) {
				state.lastCycle = now;
			}
		}
		// Give every display slot a .tri target and let RaceMenu rewrite its
		// vertices with the morphs it currently holds. No detach, no rebuild: the
		// clone keeps its identity and no frame ever shows the real armour.
		BakeResult BakeIllusion(RE::Actor* a_actor, BakeReason a_reason)
		{
			// 1.5.26: the .tri target is what makes RaceMenu morph the clone, and
			// attaching it needs no provider interface at all - skee reads it from
			// inside its own pass (BodyMorphInterface.cpp, MorphCache::UpdateMorphs
			// walks every biped object and ApplyMorphs looks for "BODYTRI" in the
			// subtree it was handed). The interface is only used to re-morph right
			// now, so 1.5.20-1.5.25 gave up on the whole mechanism whenever the
			// handshake failed - which can persist when early registration missed this plugin.
			if (!a_actor || !a_actor->Is3DLoaded()) {
				return BakeResult::kSkipped;
			}
			const auto id = a_actor->GetFormID();
			const auto mask = CoveredSlotMask(id);
			if (mask == 0) {
				return BakeResult::kSkipped;
			}
			const auto now = std::chrono::steady_clock::now();
			if (now - morphPush::g_lastBake < morphPush::kMinBakeGap) {
				return BakeResult::kRateLimited;
			}
			morphPush::g_lastBake = now;

			// Slot -> armour of the current illusion, so the .tri path can be
			// derived without searching the outfit per node.
			std::unordered_map<std::uint32_t, RE::TESObjectARMO*> bySlot;
			{
				auto service = outfit_service_get_singleton_ptr();
				if (const auto* outfit = service->inner().current_outfit_ptr(id)) {
					for (const auto* armor : outfit->armors()) {
						if (!armor) {
							continue;
						}
						const auto bits = armor->GetSlotMask().underlying();
						for (std::uint32_t bit = 0; bit < 32; ++bit) {
							if (bits & (1u << bit)) {
								bySlot.try_emplace(bit, const_cast<RE::TESObjectARMO*>(armor));
							}
						}
					}
				}
			}

			auto* race = a_actor->GetRace();
			const auto* actorBase = a_actor->GetActorBase();
			const bool female = actorBase && actorBase->GetSex() == RE::SEX::kFemale;

			const bool evidence = id != morphPush::g_evidenceActor ||
				now - morphPush::g_lastEvidence > std::chrono::seconds(5);
			const auto before = evidence && g_api ? ComputeGeometrySignature(a_actor, mask) : 0;

			std::size_t slots{ 0 };
			std::size_t targets{ 0 };
			std::size_t nodes{ 0 };
			for (const bool firstPerson : { false, true }) {
				auto* biped = a_actor->GetBiped(firstPerson).get();
				if (!biped) {
					continue;
				}
				for (std::uint32_t i = 0; i < 32; ++i) {
					if (!(mask & (1u << i))) {
						continue;
					}
					auto* node = biped->objects[i].partClone.get();
					if (!node) {
						continue;
					}
					++slots;
					if (const auto it = bySlot.find(i); it != bySlot.end()) {
						// 1.5.27: the addon the engine attached this slot with is the
						// only reliable source for the model path (BodyTriPathFor).
						if (const auto path = BodyTriPathFor(it->second, biped->objects[i].addon, race, female, firstPerson); !path.empty()) {
							// A no-op when the clone already carries one.
							AttachBodyTri(node, path);
						}
					}
					// Count what the node carries, whoever put it there: a .nif that
					// shipped with its own BODYTRI arms the slot just as well, and
					// 1.5.26 only counted the ones this module attached itself.
					if (NodeHasBodyTri(node)) {
						++targets;
					}
					// Only the immediate re-morph needs the interface. The .tri
					// target above is what makes RaceMenu morph the clone itself,
					// and that works with no interface at all.
					if (g_api) {
						morphPush::g_selfWrite.store(true, std::memory_order_release);
						g_api->ApplyVertexDiff(a_actor, node, false);
						morphPush::g_selfWrite.store(false, std::memory_order_release);
						++nodes;
					}
				}
			}

			if (morphPush::g_targetLogged.insert(id).second) {
				SKSE::log::info("MorphRefresh: {:08X} has readable BODYTRI resources on {} of {} cloned slot(s) ({}); paths only, not proof of TRI shape matching or morph movement.",
					id, targets, slots, BakeReasonText(a_reason));
			}
			if (evidence && slots > 0) {
				morphPush::g_evidenceActor = id;
				morphPush::g_lastEvidence = now;
				if (g_api) {
					const auto after = ComputeGeometrySignature(a_actor, mask);
					SKSE::log::debug("MorphRefresh: re-morphed {:08X} on {} node(s), {} .tri target(s) of {} slot(s) ({}); the vertices {}.",
						id, nodes, targets, slots, BakeReasonText(a_reason),
						before == after ? "did not change (not proof of success)" : "moved");
				} else {
					SKSE::log::debug("MorphRefresh: {:08X} has no BodyMorph interface; {} .tri target(s) of {} slot(s) stay armed for RaceMenu's own pass ({}).",
						id, targets, slots, BakeReasonText(a_reason));
				}
			}
			{
				std::lock_guard lock(g_mutex);
				morphPush::g_armed[id] = targets > 0;
			}
			if (nodes > 0) {
				AbsorbOwnWrite(a_actor);
			}
			return (nodes > 0 || targets > 0) ? BakeResult::kBaked : BakeResult::kSkipped;
		}

		bool IllusionsArmed(RE::FormID a_id)
		{
			std::lock_guard lock(g_mutex);
			const auto it = morphPush::g_armed.find(a_id);
			return it != morphPush::g_armed.end() && it->second;
		}

		void DrainPending()
		{
			BakeReason reason = BakeReason::kProvider;
			std::vector<RE::FormID> ids;
			{
				std::lock_guard lock(morphPush::g_queueMutex);
				if (morphPush::g_pending.empty()) {
					return;
				}
				ids.assign(morphPush::g_pending.begin(), morphPush::g_pending.end());
				morphPush::g_pending.clear();
				reason = morphPush::g_reason;
			}
			for (const auto id : ids) {
				auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
				if (!actor) {
					continue;
				}
				if (BakeIllusion(actor, reason) == BakeResult::kRateLimited) {
					// Keep it queued; the next callback or the poll picks it up.
					std::lock_guard lock(morphPush::g_queueMutex);
					morphPush::g_pending.insert(id);
				}
			}
		}

		void RequestBake(RE::FormID a_id, BakeReason a_reason)
		{
			{
				std::lock_guard lock(morphPush::g_queueMutex);
				morphPush::g_pending.insert(a_id);
				morphPush::g_reason = a_reason;
			}
			if (morphPush::g_taskQueued.exchange(true)) {
				return;
			}
			auto* tasks = SKSE::GetTaskInterface();
			if (!tasks) {
				morphPush::g_taskQueued.store(false);
				return;
			}
			tasks->AddTask([]() {
				morphPush::g_taskQueued.store(false);
				DrainPending();
			});
		}

		// skee v5, called once per morphed shape, on the thread RaceMenu morphs from.
		// Nothing but bookkeeping may happen here - see the block comment above.
		void OnMorphShape(void* a_refr, void* a_rootNode, void* a_geometry, void* a_skinPartition, void* a_vertexBuffer)
		{
			(void)a_geometry;
			(void)a_skinPartition;
			(void)a_vertexBuffer;
			if (!a_refr || !a_rootNode) {
				return;
			}
			if (morphPush::g_selfWrite.load(std::memory_order_acquire)) {
				return;
			}
			auto* actor = static_cast<RE::TESObjectREFR*>(a_refr)->As<RE::Actor>();
			if (!actor) {
				return;
			}
			RequestBake(actor->GetFormID(), BakeReason::kProvider);
		}

		// OBody::NotifyMorphApplied (src/Body/Body.cpp) broadcasts this right after
		// it called ApplyBodyMorphs on the actor, with the actor as sender.
		std::atomic<bool> g_raceMenuSliderPending{ false };

		void DrainRaceMenuSlider()
		{
			if (!g_raceMenuSliderPending.exchange(false, std::memory_order_acq_rel)) { return; }
			auto* ui = RE::UI::GetSingleton();
			if (g_api && ui && ui->IsMenuOpen("RaceSex Menu")) {
				if (auto* player = RE::PlayerCharacter::GetSingleton(); player && player->Is3DLoaded()) {
					RequestBake(player->GetFormID(), BakeReason::kProvider);
				}
			}
		}

		class OBodyMorphSink final : public RE::BSTEventSink<SKSE::ModCallbackEvent>
		{
		public:
			static OBodyMorphSink* GetSingleton()
			{
				static OBodyMorphSink singleton;
				return std::addressof(singleton);
			}

			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
				RE::BSTEventSource<SKSE::ModCallbackEvent>*) override
			{
				if (a_event && a_event->eventName == "RSM_SliderChange") {
					// The public UI event is not a completion callback; defer explicit
					// clone application to the main-thread queue and preserve morph values.
					g_raceMenuSliderPending.store(true, std::memory_order_release);
				}
				if (a_event && a_event->sender && a_event->eventName.c_str() &&
					std::strcmp(a_event->eventName.c_str(), "Obody_ApplyMorph") == 0) {
					if (auto* actor = a_event->sender->As<RE::Actor>()) {
						RequestBake(actor->GetFormID(), BakeReason::kProvider);
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// Safe to call from any of the provider entry points: they all run on the
		// main thread (Install, the heartbeat's task, kPostLoadGame).
		void InstallPush()
		{
			if (g_api && !morphPush::g_shapeCallbackInstalled) {
				if (g_apiVersion >= skee::kBodyMorphVersionWithShapeCallback) {
					g_api->AddMorphShapeCallback(&OnMorphShape, 0);
					morphPush::g_shapeCallbackInstalled = true;
					SKSE::log::info("MorphRefresh: RaceMenu BodyMorph v{} hands out MorphShapeCallback; the illusion is re-morphed as soon as RaceMenu writes the vertices.",
						g_apiVersion);
				} else {
					SKSE::log::info("MorphRefresh: RaceMenu BodyMorph v{} has no MorphShapeCallback (v5 added it); the .tri morph of the illusion follows the poll instead.",
						g_apiVersion);
				}
			}
			if (!morphPush::g_obodySinkInstalled) {
				if (auto* source = SKSE::GetModCallbackEventSource()) {
					source->AddEventSink(OBodyMorphSink::GetSingleton());
					morphPush::g_obodySinkInstalled = true;
					SKSE::log::debug("MorphRefresh: listening for OBody morph and RaceMenu RSM_SliderChange events; explicit clone application requires the BodyMorph interface.");
				}
			}
		}

		// Runs kVerifyDelay after a provider pass: if the meshes moved on their
		// own the clones followed and nothing else is needed; if they stand still
		// the clone is not tracked by the provider and the cycle (the only path
		// that reloads a .tri target) takes over.
		void VerifyProviderPass()
		{
			const auto now = std::chrono::steady_clock::now();
			struct Due
			{
				RE::FormID id;
				std::uint64_t before;
				int attempts;
			};
			std::vector<Due> due;
			{
				std::lock_guard lock(g_mutex);
				for (auto it = g_verify.begin(); it != g_verify.end();) {
					if (it->second.when <= now) {
						due.push_back({ it->first, it->second.before, it->second.attempts });
						it = g_verify.erase(it);
					} else {
						++it;
					}
				}
			}
			for (const auto& [id, before, attempts] : due) {
				auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
				if (!actor || !actor->Is3DLoaded()) {
					std::lock_guard lock(g_mutex);
					if (auto state = g_signatures.find(id); state != g_signatures.end()) {
						state->second.verifyActive = false;
					}
					continue;
				}
				const auto now2 = std::chrono::steady_clock::now();
				const auto after = ComputeCoveredGeometry(actor, id);
				if (after != before) {
					SKSE::log::trace("MorphRefresh: the covered geometry signature of {:08X} changed after the provider pass; individual outfit morph movement still needs verification.", id);
				} else if (attempts < kVerifyAttempts) {
					// Allow deferred geometry writes to land before comparing again.
					{
						std::lock_guard lock(g_mutex);
						g_verify[id] = VerifyState{ before, now2 + kVerifyDelay, attempts + 1 };
					}
					continue;
				} else if (IllusionsArmed(id)) {
					// A readable path does not prove that a provider traverses this clone.
					SKSE::log::debug("MorphRefresh: the provider pass left the meshes of {:08X} alone, but the illusion carries .tri targets; leaving it to the next morph pass.", id);
				} else {
					SKSE::log::debug("MorphRefresh: the provider pass left the meshes of {:08X} alone; queueing target repair without removing the illusion.", id);
					RequestBake(id, BakeReason::kProvider);
				}
				const auto baseline = ComputeSignature(actor);
				const auto providerSignature = ComputeProviderSignature(actor, nullptr);
				std::lock_guard lock(g_mutex);
				if (auto state = g_signatures.find(id); state != g_signatures.end()) {
					state->second.value = baseline;
					state->second.fired = baseline;
					state->second.actedProvider = providerSignature;
					state->second.valueSince = now2;
					state->second.lastCycle = now2;
					state->second.quietUntil = now2 + kPostCycleQuiet;
					state->second.quietFrom = now2;
					state->second.quietDrift = false;
					state->second.verifyActive = false;
					state->second.quietPasses = 0;
				}
			}
		}

		void PollOnMainThread()
		{
			{
				std::lock_guard lock(g_mutex);
				g_pollPending = false;
			}

			const auto now = std::chrono::steady_clock::now();

			// Only a split cycle (ReillusionGapMs > 0) has a second half left here.
			struct PendingRestore
			{
				RE::FormID id;
				std::string name;
			};
			std::vector<PendingRestore> restoreNow;
			{
				std::lock_guard lock(g_mutex);
				for (auto it = g_reillusion.begin(); it != g_reillusion.end();) {
					if (it->second.due <= now) {
						restoreNow.push_back({ it->first, it->second.name });
						it = g_reillusion.erase(it);
					} else {
						++it;
					}
				}
			}
			for (const auto& step : restoreNow) {
				auto* actor = RE::TESForm::LookupByID<RE::Actor>(step.id);
				if (!actor || !actor->Is3DLoaded()) {
					continue;
				}
				RestoreIllusion(actor, step.id, step.name);
			}

			VerifyProviderPass();

			// No provider gate any more: the geometry probe works without one, so
			// a RaceMenu that refuses to answer no longer disables the refresh.
			// Collect first, then release the outfit lock before refreshing.
			std::vector<RE::FormID> actors;
			{
				auto service = outfit_service_get_singleton_ptr();
				auto& inner = service->inner();
				const auto assigned = inner.list_actors();
				actors.reserve(assigned.size());
				for (const auto raw : assigned) {
					const auto id = static_cast<RE::FormID>(raw);
					if (inner.should_override(id)) {
						actors.push_back(id);
					}
				}
			}

			for (const auto id : actors) {
				auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
				if (!actor || !actor->Is3DLoaded()) {
					continue;
				}

				const auto signature = ComputeSignature(actor);
				bool anyProvider = false;
				const auto providerSignature = ComputeProviderSignature(actor, &anyProvider);
				bool changed = false;
				{
					std::lock_guard lock(g_mutex);
					const auto [it, inserted] = g_signatures.try_emplace(id);
					auto& state = it->second;
					const bool quiet = now < state.quietUntil;
					if (inserted) {
						// First sighting is the baseline, never a pass.
						state.value = signature;
						state.fired = signature;
						state.actedProvider = providerSignature;
						state.valueSince = now;
						state.lastCycle = now;
						state.quietFrom = now;
					} else if (state.value != signature) {
						// Still moving (a ramp in flight, a preset being applied,
						// or our own rebuild settling): wait for it to sit still
						// before touching the illusion.
						state.value = signature;
						state.valueSince = now;
						if (quiet) {
							// 1.5.19 absorbed every change that arrived inside the
							// window, so a slider drag that finished while it was
							// open never reached the illusion at all. Keep the
							// baseline, but remember a change that cannot be the
							// tail of our own pass and replay it once it closes.
							state.fired = signature;
							// A provider value that moved away from the one our
							// pass acted on is a real change - unless a check of
							// that pass is still in flight, in which case the move
							// is our own doing (OBody re-derives the preset every
							// time it is asked to apply) and the check is about to
							// adopt it as the new baseline. Without a provider all
							// we have is the geometry, which our own rebuild also
							// moves, so there the grace period has to decide.
							const bool providerMoved =
								anyProvider && providerSignature != state.actedProvider && !state.verifyActive;
							if (providerMoved || (!anyProvider && now - state.quietFrom > kSelfNoiseGrace)) {
								state.quietDrift = true;
							}
						}
					} else if (!quiet &&
							   (state.fired != signature || state.quietDrift) &&
							   now - state.valueSince >= kSignatureSettle &&
							   now - state.lastCycle >= kMinCycleInterval) {
						if (state.quietDrift && state.quietPasses >= kMaxQuietPasses) {
							// OBody re-derives its preset every time we touch the
							// actor, so this chain can feed itself. Take the shape
							// we can see and stop chasing it.
							SKSE::log::debug("MorphRefresh: the shape of {:08X} keeps moving after our own passes; settling on it.", id);
							state.quietDrift = false;
							state.fired = signature;
						} else {
							state.fired = signature;
							state.actedProvider = providerSignature;
							state.lastCycle = now;
							state.quietUntil = now + kPostCycleQuiet;
							state.quietFrom = now;
							if (state.quietDrift) {
								++state.quietPasses;
							} else {
								state.quietPasses = 0;
							}
							state.quietDrift = false;
							state.verifyActive = false;
							changed = true;
						}
					}
				}
				if (!changed) {
					continue;
				}

				SKSE::log::debug("MorphRefresh: the body morphs of {:08X} changed; following them.", id);

				// 1.5.19 provider-first pass. A body morph provider rebuilds every
				// morphable mesh of the actor, the illusion clones included, so one
				// more ApplyBodyMorphs is usually all "the illusion follows the
				// slider" needs. It costs one body rebuild instead of the two full
				// worn rebuilds of the cycle - the ~600 ms main-thread stall the
				// OBody preset switches showed - and never exposes the real gear.
				// A missing target is repaired in place, never by exposing real gear.
				EnsureProvidersForUse();
				if (Settings::Get().morphProviderFirst && ProviderCanMorph()) {
					const auto before = ComputeCoveredGeometry(actor, id);
					ApplyMorphs(actor);
					// The check runs against the slots the illusion covers. A
					// whole-actor signature always moves while a slider is being
					// dragged (the body underneath is rewritten too), which made
					// 1.5.19 conclude the clones had followed when they had not.
					std::lock_guard lock(g_mutex);
					g_verify[id] = VerifyState{ before, now + kVerifyDelay, 0 };
					// No re-baseline yet: VerifyProviderPass owns the next baseline,
					// otherwise this settle window would swallow the very change it
					// is waiting for.
					if (auto state = g_signatures.find(id); state != g_signatures.end()) {
						state->second.lastCycle = now;
						state->second.quietUntil = now + kPostCycleQuiet;
						state->second.quietFrom = now;
						state->second.verifyActive = true;
					}
					continue;
				}

				// 1.5.27: when the clones carry a .tri target, the next morph pass
				// (RaceMenu on the next slider write, OBody, SLIF) writes them. The
				// cycle would take the illusion off for one engine pass - the flash
				// this module exists to remove - and hand back clones that start at
				// the base shape again.
				if (IllusionsArmed(id)) {
					SKSE::log::debug("MorphRefresh: the illusion of {:08X} carries .tri targets; leaving the clones to the next morph pass instead of cycling.", id);
					const auto armedBaseline = ComputeSignature(actor);
					std::lock_guard lock(g_mutex);
					if (auto state = g_signatures.find(id); state != g_signatures.end()) {
						state->second.value = armedBaseline;
						state->second.fired = armedBaseline;
						state->second.actedProvider = providerSignature;
						state->second.valueSince = now;
						state->second.lastCycle = now;
						state->second.quietUntil = now + kPostCycleQuiet;
						state->second.quietFrom = now;
						state->second.quietDrift = false;
						state->second.verifyActive = false;
						state->second.quietPasses = 0;
					}
					continue;
				}

				RequestBake(id, BakeReason::kProvider);

				// Re-baseline the observer; target repair never removes the outfit.
				const auto baseline = ComputeSignature(actor);
				{
					std::lock_guard lock(g_mutex);
					if (auto state = g_signatures.find(id); state != g_signatures.end()) {
						state->second.value = baseline;
						state->second.fired = baseline;
						state->second.actedProvider = providerSignature;
						state->second.valueSince = now;
						state->second.lastCycle = now;
						state->second.quietUntil = now + kPostCycleQuiet;
						state->second.quietFrom = now;
						state->second.quietDrift = false;
						state->second.verifyActive = false;
						state->second.quietPasses = 0;
					}
				}
			}

			// RaceMenu rebuilds the meshes asynchronously, so applying the stored
			// morphs right after the equipment refresh can still hit the old
			// clone. Run once shortly after the hook rebuilt the slots.
			std::vector<RE::FormID> applyNow;
			{
				std::lock_guard lock(g_mutex);
				for (auto it = g_pendingApply.begin(); it != g_pendingApply.end();) {
					if (it->second <= now) {
						applyNow.push_back(it->first);
						it = g_pendingApply.erase(it);
					} else {
						++it;
					}
				}
			}
			for (const auto id : applyNow) {
				{
					std::lock_guard lock(g_mutex);
					g_applyScheduled.erase(id);
				}
				if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
					ApplyMorphs(actor);
				}
			}
		}

		bool TryRaceMenu(const SKSE::MessagingInterface* a_messaging, bool a_quiet = false)
		{
			// SKSE only dispatches to subscribers of our sender handle.
			// A catch-all registered before our handle existed can miss it.
			constexpr const char* kReceivers[] = { "skee64", "skee", nullptr };
			const char* answered = nullptr;
			skee::InterfaceExchangeMessage exchange{};
			for (const auto* receiver : kReceivers) {
				a_messaging->Dispatch(skee::InterfaceExchangeMessage::kMessage_ExchangeInterface,
					&exchange, sizeof(exchange), receiver);
				if (exchange.interfaceMap) {
					answered = receiver ? receiver : "(broadcast)";
					break;
				}
			}
			if (!exchange.interfaceMap) {
				if (!a_quiet) {
					SKSE::log::info("MorphRefresh: RaceMenu did not answer the interface exchange. The installed RaceMenu build and messaging contract must be verified; do not add unsupported INI settings. BODYTRI paths alone do not prove morph success.");
				}
				return false;
			}

			auto* candidate = static_cast<skee::IBodyMorphInterface*>(
				exchange.interfaceMap->QueryInterface("BodyMorph"));
			if (!candidate) {
				SKSE::log::info("MorphRefresh: RaceMenu has no BodyMorph interface.");
				return false;
			}
			const auto version = candidate->GetVersion();
			if (version < 4) {
				SKSE::log::warn("MorphRefresh: RaceMenu BodyMorph v{} is older than v4; ignoring it.", version);
				return false;
			}

			g_api = candidate;
			g_apiVersion = version;
			SKSE::log::info("MorphRefresh: RaceMenu BodyMorph v{} acquired through {}; explicit clone morph application is available (movement still needs verification).",
				version, answered);
			return true;
		}

		bool TryOBody(const SKSE::MessagingInterface* a_messaging, bool a_quiet = false)
		{
			obody::IPluginInterface* candidate = nullptr;
			obody::RequestPluginInterface request{};
			request.version = obody::PluginAPIVersion::v1;
			request.pluginInterface = &candidate;
			request.readinessEventListener = &g_obodyReadiness;

			a_messaging->Dispatch(obody::RequestPluginInterface::type,
				&request, sizeof(request), "OBody");
			if (!candidate) {
				// Same receiver-name pitfall as skee; OBody answers a broadcast.
				a_messaging->Dispatch(obody::RequestPluginInterface::type,
					&request, sizeof(request), nullptr);
			}
			if (!candidate) {
				if (!a_quiet) {
					SKSE::log::debug("MorphRefresh: OBody plugin API is unavailable (OBody is not installed or is older than 4.4.0).");
				}
				return false;
			}

			g_obody = candidate;
			SKSE::log::info("MorphRefresh: OBody plugin API v1 acquired; preset changes will rebuild the illusion.");
			return true;
		}

		bool TrySLIF(const SKSE::MessagingInterface* a_messaging, bool a_quiet = false)
		{
			// The shipped module is BodyInflationNG.dll; repackaged and older
			// builds use the short names. All of them are probed, and the C export
			// is preferred because SKSE's receiver-name resolution can miss it.
			constexpr const char* kModules[] = { "BodyInflationNG.dll", "SLIFNG.dll", "SLIF.dll" };
			constexpr const char* kReceivers[] = { "SLIFNG", "BodyInflationNG", "SLIF" };
			for (const auto* moduleName : kModules) {
				auto* module = ::GetModuleHandleA(moduleName);
				if (!module) {
					continue;
				}
				if (auto* get = reinterpret_cast<slif::GetQueryInterfaceFn>(
						::GetProcAddress(module, slif::kExportName))) {
					g_slif = get();
				}
				if (g_slif) {
					break;
				}
				// SLIF NG 0.4.17 and older only serve the messaging handshake.
				for (const auto* receiver : kReceivers) {
					slif::InterfaceExchangeMessage exchange{};
					a_messaging->Dispatch(slif::InterfaceExchangeMessage::kMessageType,
						&exchange, sizeof(exchange), receiver);
					if (exchange.query) {
						g_slif = exchange.query;
						break;
					}
				}
				if (!g_slif) {
					slif::InterfaceExchangeMessage exchange{};
					a_messaging->Dispatch(slif::InterfaceExchangeMessage::kMessageType,
						&exchange, sizeof(exchange), nullptr);
					g_slif = exchange.query;
				}
				if (g_slif) {
					break;
				}
			}
			if (!g_slif) {
				if (!a_quiet) {
					SKSE::log::debug("MorphRefresh: SLIF NG is not installed (no {} export and no handshake answer); inflation follows the geometry probe only.",
						slif::kExportName);
				}
				return false;
			}
			SKSE::log::info("MorphRefresh: SLIF NG query interface v{} acquired ({} tracked actors); inflation follows the slider and geometry probes.",
				g_slif->Version(), g_slif->TrackedActorCount());
			return true;
		}
	}

	bool Available()
	{
		return g_api != nullptr || g_slif != nullptr ||
			(g_obody != nullptr && g_obodyReady.load(std::memory_order_acquire));
	}

	bool RunIllusionCycle(RE::Actor* a_actor, const char* a_reason)
	{
		return CycleIllusion(a_actor, a_reason);
	}

	void ScheduleReillusion(RE::Actor* a_actor)
	{
		CycleIllusion(a_actor, "the outfit was re-applied");
	}

	void Install()
	{
		if (g_installed) {
			return;
		}
		g_installed = true;

		auto* messaging = SKSE::GetMessagingInterface();
		if (!messaging) {
			SKSE::log::debug("MorphRefresh: SKSE messaging is unavailable; the .tri morph refresh stays off.");
			return;
		}

		// Keep messaging out of skinning callbacks. Queueing cannot fix a missing
		// subscription caused by an early catch-all registration in SKSE 2.0.20.
		QueueProviderRetry();
		SKSE::log::info("MorphRefresh: readable BODYTRI targets are attached to illusion clones. Older RaceMenu builds may only traverse real equipped armour; explicit clone morph application requires the BodyMorph interface.");
	}

// RaceMenu may only answer the interface exchange after its own data loaded
// work on 1.5.97, so a failure at kDataLoaded is not final - and it is not
// final for the whole session either: this runs from the heartbeat, from
// kPostLoadGame and lazily from EnsureProvidersForUse right before the
// interface is used. It never gives up, only slows down.
// The diagnostic the SLIF NG author uses, and the one 1.5.18 was missing: a
// silent exchange means something different depending on whether skee is in
// the process at all. Without this line "RaceMenu did not answer" reads as
// "RaceMenu is not installed" even when skee64.dll is loaded and working.
void ReportRaceMenuSilence()
{
	static auto lastReport = std::chrono::steady_clock::time_point{};
	static int reports{ 0 };
	const auto now = std::chrono::steady_clock::now();
	if (lastReport != std::chrono::steady_clock::time_point{} && now - lastReport < std::chrono::minutes(5)) {
		return;
	}
	lastReport = now;
	if (++reports > 8) {
		return;
	}
	constexpr const char* kSkeeModules[] = { "skee64.dll", "skee.dll", "skeevr.dll" };
	const char* found = nullptr;
	for (const auto* moduleName : kSkeeModules) {
		if (::GetModuleHandleA(moduleName)) {
			found = moduleName;
			break;
		}
	}
	if (found) {
		SKSE::log::info("MorphRefresh: {} is in the process but RaceMenu still has not handed out the BodyMorph interface; kept asking ({} tries). Readable BODYTRI paths are not sufficient on versions that only traverse real equipped armour; explicit clone application is unavailable.",
			found, g_providerRetries);
	} else {
		SKSE::log::info("MorphRefresh: no known skee module in the process. No BodyMorph mesh interface is available; geometry observation alone cannot apply morphs to illusion clones.");
	}
}

void RetryProviders()
{
	auto* messaging = SKSE::GetMessagingInterface();
	if (!messaging) {
		return;
	}
	if (g_api == nullptr) {
		TryRaceMenu(messaging, true);
	}
	if (g_obody == nullptr) {
		TryOBody(messaging, true);
	}
	if (g_slif == nullptr) {
		TrySLIF(messaging, true);
	}
	// Late arrival: the interface that just answered still needs its morph
	// notification hooked up.
	InstallPush();
	if (g_api == nullptr) {
		ReportRaceMenuSilence();
	}
}

	void Reset()
	{
		std::lock_guard lock(g_mutex);
		g_signatures.clear();
		g_forced.clear();
		g_pendingApply.clear();
		g_applyScheduled.clear();
		g_verify.clear();
		g_reillusion.clear();
		{
			std::lock_guard queueLock(morphPush::g_queueMutex);
			morphPush::g_pending.clear();
		}
		morphPush::g_lastBake = {};
		morphPush::g_lastEvidence = {};
		morphPush::g_evidenceActor = 0;
		morphPush::g_targetLogged.clear();
		morphPush::g_armed.clear();
		g_triProbeLogged.clear();
		g_triPathCache.clear();
		g_pollPending = false;
		g_raceMenuSliderPending.store(false, std::memory_order_release);
		g_lastPoll = {};
		g_lastProviderRetry = {};
		g_providerRetries = 0;
	}

	void Tick()
	{
		const auto now = std::chrono::steady_clock::now();
		bool retryProviders = false;
		{
			// Tick runs on the input thread while Reset can run on the main
			// thread during a load; keep the throttle and the pending flag
			// under the same lock.
			std::lock_guard lock(g_mutex);
			const bool incomplete = g_api == nullptr || g_obody == nullptr || g_slif == nullptr;
			// Never stop asking: RaceMenu on 1.5.97 can stay silent for the whole
			// main menu and answer only once the game is running. The fast cadence
			// covers the load sequence, the slow one keeps the question alive for
			// the rest of the session at no measurable cost.
			const auto interval = g_providerRetries < 8 ? kProviderRetryInterval : kProviderRetrySlowInterval;
			if (incomplete && now - g_lastProviderRetry >= interval) {
				g_lastProviderRetry = now;
				if (g_providerRetries < 1000) {
					++g_providerRetries;
				}
				retryProviders = true;
			}
			// The geometry probe needs no provider at all, so the poll always
			// runs; this flag only keeps the input thread from queueing work.
			if (g_pollPending || now - g_lastPoll < kPollInterval) {
				if (!retryProviders) {
					return;
				}
			} else {
				g_pollPending = true;
				g_lastPoll = now;
			}
		}

		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks) {
			std::lock_guard lock(g_mutex);
			g_pollPending = false;
			return;
		}
		tasks->AddTask([retryProviders]() {
			if (retryProviders) {
				RetryProviders();
			}
			// Safety net for a re-morph that got rate limited or whose task was
			// dropped: the poll picks the actor up again.
			DrainRaceMenuSlider();
			DrainPending();
			PollOnMainThread();
		});
	}

	bool ShouldForceRebuild(RE::FormID a_actor)
	{
		std::lock_guard lock(g_mutex);
		const auto it = g_forced.find(a_actor);
		if (it == g_forced.end()) {
			return false;
		}
		if (it->second <= std::chrono::steady_clock::now()) {
			g_forced.erase(it);
			return false;
		}
		return true;
	}

	void NotifySlotApplied(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return;
		}
		// 1.5.26: run even without the interface. The clones the skinning hook
		// just attached carry no .tri target until this pass runs, and without
		// that target RaceMenu's own morph pass skips them entirely - which is
		// the "the illusion does not follow the slider" symptom.
		RequestBake(a_actor->GetFormID(), BakeReason::kRebuilt);
	}

	void NotifyRebuilt(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return;
		}
		// OBody reapply regenerates preset values; it is not a mesh-only refresh.
		if (!g_api) {
			return;
		}
		std::lock_guard lock(g_mutex);
		const auto id = a_actor->GetFormID();
		if (!g_applyScheduled.insert(id).second) {
			return;
		}
		g_pendingApply[id] = std::chrono::steady_clock::now() + kApplyDelay;
		SKSE::log::trace("MorphRefresh: queued the body morph pass for {:08X}.", id);
	}
}
