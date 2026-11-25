#include "OutfitSystem.h"

#include "bindings.h"
#include "NGHotkeys.h"
#include "GameState.h"
#include "I18n.h"
#include "MenuUI.h"
#include "AutoSwitch.h"
#include "MorphRefresh.h"
#include "SceneOutfits.h"
#include "Utility.h"
#include "NGFeatures.h"
#include "compat/ModCompat.h"
#include "hooking/Hooks.hpp"
#include "core/GoldenItems.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <cstring>
#include <exception>
#include <Windows.h>

extern "C" {
void InitializeLog() {
    auto path = SKSE::log::log_directory();
    if (!path) {
        SKSE::log::critical("Failed to find the standard logging directory");
        return;
    }

    *path /= fmt::format("{}.log"sv, Plugin::NAME);
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

    // Activate logging of everything until we load the log setting.
    auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
    log->set_level(spdlog::level::trace);
    log->flush_on(spdlog::level::trace);

    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%H:%M:%S.%f] %t [%^%l%$] %s:%#: %v"s);

    // Load the actual log setting we should use.
    auto level = spdlog::level::info;
    bool deepLogEnabled = settings_extra_logging_enabled();
    if (deepLogEnabled) {
        level = spdlog::level::trace;
    }

    // Set the actual log setting from hereon.
    spdlog::default_logger()->set_level(level);
    spdlog::default_logger()->flush_on(level);
}

namespace {
    constexpr std::size_t kTrampolineSize = 64 * 1024;

    // Reserves executable memory within +-1GB of the .text segment so that every
    // 5-byte branch we write can reach it with a rel32 displacement.
    //
    // SKSE's own branch pool (SKSE::AllocTrampoline / InitInfo::trampoline) is
    // deliberately not used: it aborts the process when the allocation fails, and
    // that path has been observed to kill 1.5.97 at startup. VirtualAlloc cannot
    // fail silently here, so a failure only disables the skinning hooks.
    void* AllocateNearText(std::size_t a_size)
    {
        const auto text = REL::Module::get().segment(REL::Segment::textx);
        const auto textBegin = reinterpret_cast<std::uintptr_t>(text.pointer<std::byte>());
        const auto textEnd = textBegin + text.size();
        constexpr std::uintptr_t kMaxDistance = 0x40000000;  // 1GB, rel32 allows 2GB
        const std::uintptr_t lowest = textBegin > kMaxDistance ? textBegin - kMaxDistance : 0x10000;
        const std::uintptr_t highest = textEnd + kMaxDistance;

        auto scan = [a_size](std::uintptr_t a_from, std::uintptr_t a_to) -> void* {
            std::uintptr_t addr = a_from & ~std::uintptr_t{ 0xFFFF };
            while (addr < a_to) {
                MEMORY_BASIC_INFORMATION mbi{};
                if (::VirtualQuery(reinterpret_cast<const void*>(addr), &mbi, sizeof(mbi)) == 0) {
                    return nullptr;
                }
                const auto regionBase = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
                const auto regionSize = static_cast<std::uintptr_t>(mbi.RegionSize);
                const auto regionEnd = regionBase + regionSize;
                if (mbi.State == MEM_FREE && regionSize >= a_size) {
                    const auto candidate = (regionBase + 0xFFFF) & ~std::uintptr_t{ 0xFFFF };
                    if (candidate + a_size <= regionEnd) {
                        if (auto* mem = ::VirtualAlloc(reinterpret_cast<void*>(candidate), a_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)) {
                            return mem;
                        }
                    }
                }
                if (regionEnd <= addr) {
                    break;
                }
                addr = regionEnd;
            }
            return nullptr;
        };

        if (auto* mem = scan(textEnd, highest)) {
            return mem;
        }
        return scan(lowest, textBegin);
    }
}

bool InitializeTrampolines() {
    if (Hooking::g_localTrampoline) {
        return true;
    }

    auto* mem = AllocateNearText(kTrampolineSize);
    if (!mem) {
        LOG(critical, "Could not reserve {} bytes of executable memory near the game code; the skinning hooks stay disabled.", kTrampolineSize);
        return false;
    }

    auto* trampoline = new SKSE::Trampoline("local");
    trampoline->set_trampoline(mem, kTrampolineSize, [](void* a_mem, std::size_t) {
        ::VirtualFree(a_mem, 0, MEM_RELEASE);
    });
    Hooking::g_localTrampoline = trampoline;
    Hooking::g_branchTrampoline = trampoline;

    LOG(debug, "Trampoline ready: {} bytes at rva 0x{:X} ({} free).",
        trampoline->capacity(), reinterpret_cast<std::uintptr_t>(mem) - REL::Module::get().base(), trampoline->free_size());
    return true;
}

void ApplySkinningHooks() {
    if (!SOS::Settings::Get().installHooks) {
        LOG(warn, "Skinning hooks are disabled by [Debug] InstallHooks=0; appearance override stays off.");
        return;
    }
    if (!InitializeTrampolines()) {
        LOG(critical, "Skinning hooks skipped: no trampoline.");
        return;
    }

    Hooking::ApplyPlayerSkinningHooks();
    LOG(info, "Skinning hooks applied.");
}

void SetupPapyrus() {
    auto* papyrus = SKSE::GetPapyrusInterface();
    if (!papyrus || !papyrus->Register(OutfitSystem::RegisterPapyrus)) {
        LOG(critical, "Failed to register the Papyrus functions.");
        return;
    }
    LOG(info, "Papyrus functions registered.");
}

void SetupEvents() {
    if (!OutfitSystem::RegisterEvents()) {
        LOG(err, "Failed to register the game event sinks.");
        return;
    }
    LOG(info, "Game event sinks registered.");
}

}// namespace

namespace {
    // Co-save record. The signature is the one used by the original mod ("AAOS"),
    // so that existing saves keep a single record slot and the old records are
    // recognised (and gracefully ignored, they are protobuf).
    constexpr std::uint32_t kCoSaveSignature = 'AAOS';
    // 7 = JSON payload. Versions 4..6 were protobuf and cannot be read anymore.
    constexpr std::uint32_t kCoSaveVersion = 7;
    constexpr std::uint32_t kLastLegacyVersion = 6;
    // Versions 4..6 are protobuf records written by the original mod and are
    // migrated on load; anything older is not supported.
    constexpr std::uint32_t kFirstLegacyVersion = 4;

    void SaveCallback(SKSE::SerializationInterface* a_intfc) {
        if (!a_intfc) {
            return;
        }

        auto service = outfit_service_get_singleton_ptr();
        const auto data = service->inner().save_json_c();

        if (!a_intfc->OpenRecord(kCoSaveSignature, kCoSaveVersion)) {
            LOG(err, "Failed to open the co-save record.");
            return;
        }
        if (!a_intfc->WriteRecordData(data.data(), static_cast<std::uint32_t>(data.size()))) {
            LOG(err, "Failed to write the co-save record.");
            return;
        }
        LOG(info, "Saved {} outfits to the co-save ({} bytes).", service->inner().outfit_count(), data.size());
    }

    void LoadCallback(SKSE::SerializationInterface* a_intfc) {
        if (!a_intfc) {
            return;
        }

        std::uint32_t type = 0;
        std::uint32_t version = 0;
        std::uint32_t length = 0;
        while (a_intfc->GetNextRecordInfo(type, version, length)) {
            if (type != kCoSaveSignature) {
                continue;
            }
            if (version > kCoSaveVersion) {
                LOG(warn, "Ignoring co-save record v{}: it was written by a newer version of the mod.", version);
                continue;
            }
            if (length == 0) {
                continue;
            }

            std::string buffer(length, '\0');
            if (a_intfc->ReadRecordData(buffer.data(), length) != length) {
                LOG(err, "Co-save record is truncated ({} bytes expected).", length);
                continue;
            }

            auto service = outfit_service_get_mut_singleton_ptr();
            if (version <= kLastLegacyVersion) {
                if (version < kFirstLegacyVersion) {
                    LOG(warn, "Ignoring co-save record v{}: it is older than the oldest supported format (v{}).", version, kFirstLegacyVersion);
                    continue;
                }
                LOG(info, "Migrating a v{} record written by the original mod ({} bytes).", version, length);
                if (service->inner().replace_with_legacy_proto(buffer.data(), buffer.size(), version, a_intfc)) {
                    LOG(debug, "Co-save migration done; the next save will be written as v{}.", kCoSaveVersion);
                } else {
                    LOG(err, "Co-save migration failed; the outfit data was reset.");
                }
                continue;
            }

            service->inner().replace_with_json_data(rust::Str{ buffer.data(), buffer.size() }, a_intfc);
        }
    }

    // The outfit refresh has to wait until the game has actually built the
    // player's 3D; running it inside kPostLoadGame hangs 1.5.97 (the biped data
    // is still being filled in).
    //
    // That wait must never be a task that queues itself again. SKSE drains the task
    // queue until it is empty, so a task that re-adds itself while it is not ready
    // keeps the queue busy forever: the frame never ends and the game freezes with no
    // rendering and no cursor (1.5.62 did exactly that - a load the game refused
    // because a plugin was missing was enough to freeze it).
    // The wait therefore lives on the 11 ms wakeup timer: it posts at most one cheap
    // probe task every kPostLoadProbeIntervalMs until the player's 3D exists, and
    // stops after kPostLoadGiveUpMs. A probe that finds the 3D still missing simply
    // returns; the timer owns the retry.
    constexpr std::uint64_t kPostLoadProbeIntervalMs = 250;
    constexpr std::uint64_t kPostLoadProbeStaleMs = 2000;
    constexpr std::uint64_t kPostLoadGiveUpMs = 120000;

    std::atomic<bool> g_postLoadPending{ false };
    std::atomic<bool> g_postLoadProbeOutstanding{ false };
    std::atomic<std::uint64_t> g_postLoadSince{ 0 };
    std::atomic<std::uint64_t> g_postLoadLastProbe{ 0 };
    std::atomic<unsigned> g_postLoadProbes{ 0 };

    std::uint64_t NowMs() noexcept {
        return static_cast<std::uint64_t>(::GetTickCount64());
    }

    void PostPostLoadProbe()
    {
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) {
            g_postLoadPending.store(false);
            return;
        }
        g_postLoadLastProbe.store(NowMs());
        g_postLoadProbeOutstanding.store(true);
        g_postLoadProbes.fetch_add(1);
        tasks->AddTask([]() {
            g_postLoadProbeOutstanding.store(false);
            if (!g_postLoadPending.load()) {
                return;
            }
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || !player->Is3DLoaded()) {
                // The wakeup timer posts the next probe; a task must not re-add itself.
                return;
            }
            g_postLoadPending.store(false);
            LOG(debug, "Player 3D is ready after {} probe(s); refreshing the outfits.", g_postLoadProbes.load());
            CreateRefreshAllArmorsTask();
        });
    }

    void BeginPostLoadRefresh()
    {
        g_postLoadSince.store(NowMs());
        g_postLoadLastProbe.store(0);
        g_postLoadProbes.store(0);
        g_postLoadPending.store(true);
        PostPostLoadProbe();
    }

    void CancelPostLoadRefresh()
    {
        g_postLoadPending.store(false);
        g_postLoadProbeOutstanding.store(false);
    }

    // Runs on the Win32 timer thread every 11 ms, so it must not touch game data.
    void ServicePostLoadRefresh() noexcept
    {
        if (!g_postLoadPending.load()) {
            return;
        }
        const auto now = NowMs();
        const auto since = g_postLoadSince.load();
        if (now - since >= kPostLoadGiveUpMs) {
            g_postLoadPending.store(false);
            LOG(warn, "The player's 3D never became ready (a load that this game build refused leaves nothing to refresh); the outfit is refreshed on the next equipment change instead.");
            return;
        }
        const auto last = g_postLoadLastProbe.load();
        // A probe that is still queued means the game thread has not reached it yet;
        // the stale cap keeps one lost probe from stopping the wait for good.
        if (g_postLoadProbeOutstanding.load() && now - last < kPostLoadProbeStaleMs) {
            return;
        }
        if (now - last < kPostLoadProbeIntervalMs) {
            return;
        }
        PostPostLoadProbe();
    }

    void RevertCallback(SKSE::SerializationInterface* a_intfc) {
        (void)a_intfc;
        // Revert fires at the start of every load; keep the hooks out of the
        // way until kPostLoadGame.
        SOS::GameState::SetLoading(true);
        auto service = outfit_service_get_mut_singleton_ptr();
        service->inner().replace_with_new();
        service->inner().check_consistency();
        SOS::AutoSwitch::Reset();
        SOS::MorphRefresh::Reset();
        SOS::SceneOutfits::Reset();
        CancelPostLoadRefresh();
        LOG(debug, "Reverted to a fresh outfit state.");
    }

    void CALLBACK WakeRefreshQueues(PTP_CALLBACK_INSTANCE, void*, PTP_TIMER) noexcept
    {
        try {
            ServicePostLoadRefresh();
            if (!SOS::GameState::IsLoading()) {
                SOS::MorphRefresh::Tick();
            }
        } catch (...) {
            // A failed task allocation must not unwind through the Win32 callback.
        }
    }

    void StartRefreshWakeup()
    {
        // SKSE retains the plugin for the game process lifetime after Load succeeds.
        static const bool started = [] {
            if (!SKSE::GetTaskInterface()) {
                return false;
            }
            const auto timer = CreateThreadpoolTimer(WakeRefreshQueues, nullptr, nullptr);
            if (!timer) {
                LOG(warn, "Refresh wakeup could not start (Win32 {}); retaining input fallback.", GetLastError());
                return false;
            }
            constexpr DWORD periodMs = 11;
            constexpr auto firstDue = static_cast<ULONGLONG>(-static_cast<LONGLONG>(periodMs) * 10000);
            FILETIME due{static_cast<DWORD>(firstDue), static_cast<DWORD>(firstDue >> 32)};
            SetThreadpoolTimer(timer, &due, periodMs, 0);
            LOG(info, "Refresh wakeup: independent timer active; game-thread checks and input fallback retained.");
            return true;
        }();
        (void)started;
    }

    void HandleMessage(SKSE::MessagingInterface::Message* a_msg) {
        if (!a_msg) {
            return;
        }

        switch (a_msg->type) {
        case SKSE::MessagingInterface::kPostPostLoad:
            LOG(info, "RaceMenu interface probe phase: PostPostLoad.");
            SOS::MorphRefresh::ProbeRaceMenuInterface();
            break;
        case SKSE::MessagingInterface::kDataLoaded:
            // Everything else is loaded now, so trampolines and hooks are safe to set up.
            SOS::GameState::SetLoading(false);
            SOS::Settings::Get().WarmUp();
            // Golden items: the list has a file of its own now. The legacy ini
            // list is lifted into it once (the ini key is marked as migrated and
            // is ignored from then on) so an existing setup keeps its pieces.
            // A broken or half-written golden items store must never take the game
            // down while it is starting: the list is optional, the game is not.
            // Any exception raised in here is reported and the session continues
            // with an empty golden list.
            try {
                SOS::Golden::LoadFromDisk();
                if (!SOS::Settings::Get().goldenIniMigrated) {
                    SOS::Golden::ApplyLegacyEntries(SOS::Settings::Get().goldenItems);
                    SOS::Golden::SaveToDisk();
                    SOS::Settings::Get().goldenItems.clear();
                    SOS::Settings::Get().goldenIniMigrated = true;
                    SOS::Settings::Get().Save();
                }
                SOS::Golden::WarmUp();
            } catch (const std::exception& e) {
                LOG(critical, "Golden items: the store could not be loaded ({}); the golden list stays empty for this session.", e.what());
            } catch (...) {
                LOG(critical, "Golden items: the store could not be loaded; the golden list stays empty for this session.");
            }
            ApplySkinningHooks();
            SetupEvents();
            SOS::AutoSwitch::Install();
            LOG(info, "RaceMenu interface probe phase: DataLoaded.");
            SOS::MorphRefresh::ProbeRaceMenuInterface();
            SOS::MorphRefresh::Install();
            SOS::SceneOutfits::Install();
            SOS::Compat::Install();
            SOS::I18n::Init();
            LOG(debug, "Language: {}.", SOS::I18n::Status());
            SOS::Hotkeys::Install();
            StartRefreshWakeup();
            SOS::Menu::Register();
            // The options page of the old MCM toggled the quickslot power through
            // a Papyrus quest; do it natively so it also works without the quest.
            SOS::Features::ApplyQuickslotSpell();
            break;
        case SKSE::MessagingInterface::kNewGame:
        case SKSE::MessagingInterface::kPreLoadGame: {
            SOS::GameState::SetLoading(true);
            LOG(debug, "New game / pre-load: outfit data reset, hooks paused until the load finishes.");
            SOS::Settings::Get().InvalidateResolvedForms();
            SOS::Settings::Get().WarmUp();
            auto service = outfit_service_get_mut_singleton_ptr();
            service->inner().replace_with_new();
            service->inner().check_consistency();
            SOS::AutoSwitch::Reset();
            SOS::MorphRefresh::Reset();
            SOS::SceneOutfits::Reset();
            CancelPostLoadRefresh();
            SOS::Compat::Reload();
            if (a_msg->type == SKSE::MessagingInterface::kNewGame) {
                // A new game starts with a fresh player who owns no spells.
                SOS::GameState::SetLoading(false);
                SOS::Features::ApplyQuickslotSpell();
            }
            break;
        }
        case SKSE::MessagingInterface::kPostLoadGame:
            // The player skin is built during load, re-run it so the outfit shows up.
            // The refresh itself is deferred until the player's 3D exists.
            // Resolve the golden items while the skinning hooks are still paused:
            // flipping the loading flag first lets the render thread read the set
            // while it is being rebuilt.
            SOS::Settings::Get().InvalidateResolvedForms();
            SOS::Settings::Get().WarmUp();
            SOS::GameState::SetLoading(false);
            LOG(debug, "Post-load: hooks resumed; the outfit refresh waits for the player's 3D.");
            SOS::Compat::Reload();
			// The save is loaded and skee/RaceMenu have had their own data-loaded
			// pass by now; retry the interface exchange right here instead of
			// waiting for a timer that may have expired in the main menu.
			SOS::MorphRefresh::RetryProviders();
            SOS::Features::ApplyQuickslotSpell();
            BeginPostLoadRefresh();
            break;
        default:
            break;
        }
    }

    // An exception must never escape an SKSE callback: the game treats it as an
    // unhandled exception and closes. Every phase is reported here and the rest
    // of that message is skipped instead.
    void MessageHandler(SKSE::MessagingInterface::Message* a_msg) {
        try {
            HandleMessage(a_msg);
        } catch (const std::exception& e) {
            LOG(critical, "Message handler: an exception escaped ({}); the rest of this message is skipped.", e.what());
        } catch (...) {
            LOG(critical, "Message handler: an exception escaped; the rest of this message is skipped.");
        }
    }
}

extern "C" {
bool plugin_main(const SKSE::LoadInterface* a_skse) {
    // CommonLibSSE-NG writes "<plugin name>.log" and one line into it by default
    // (SKSE::InitInfo::log). That left a stray SkyrimOutfitSystemNG.log beside the
    // real OutfitSystemNG.log, so the default logger is turned off and
    // InitializeLog() below stays the only owner of the log file.
    SKSE::Init(a_skse, SKSE::InitInfo{ .log = false });

    SOS::Settings::Get().Load();
    InitializeLog();

    LOG(info, "{} v{} is loading (runtime {}, {} build).", Plugin::NAME,
        Plugin::VERSION.string(), REL::Module::get().version().string(),
        REL::Module::IsAE() ? "AE" : "SE");

    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(MessageHandler)) {
        LOG(critical, "Failed to register the messaging listener.");
        return false;
    }

    auto* serialization = SKSE::GetSerializationInterface();
    if (!serialization) {
        LOG(critical, "Failed to get the serialization interface.");
        return false;
    }
    serialization->SetUniqueID(0x634F6674);// "cOft", same as the original mod
    serialization->SetSaveCallback(SaveCallback);
    serialization->SetLoadCallback(LoadCallback);
    serialization->SetRevertCallback(RevertCallback);

    SetupPapyrus();

    LOG(info, "Plugin loaded, waiting for data to be loaded.");
    return true;
}

DllExport bool SKSEPlugin_Load(const SKSE::LoadInterface* a_skse) {
    return plugin_main(a_skse);
}
}
