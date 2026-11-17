param(
    [string] $VcVars = 'C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
)
$ErrorActionPreference = 'Stop'
# Compile only an isolated mock, never the plugin. No Skyrim/SKSE runtime is loaded.
$work = Join-Path $PSScriptRoot '../build/scene-outfit-queue-regression'
$source = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot '../src/cpp/SceneOutfits.cpp')
$start = $source.IndexOf('enum class SceneEvent')
$end = $source.IndexOf('ModEventSink g_sink;', [Math]::Max(0, $start))
if ($start -lt 0 -or $end -le $start) { throw 'Production scene queue/dispatch/sink not found' }
$queueAndSink = $source.Substring($start, $end - $start)
$tickStart = $source.IndexOf('void Tick()')
$tickEnd = $source.IndexOf('bool Install()', [Math]::Max(0, $tickStart))
if ($tickStart -lt 0 -or $tickEnd -le $tickStart) { throw 'Production Tick not found' }
$tick = $source.Substring($tickStart, $tickEnd - $tickStart)
$resetMatch = [regex]::Match($source, '(?s)void Reset\(\)\s*\{[^{}]*\}')
if (-not $resetMatch.Success) { throw 'Production Reset not found (or structure changed)' }
if (-not (Test-Path -LiteralPath $VcVars)) { throw "MSVC environment script not found: $VcVars" }
New-Item -ItemType Directory -Path $work -Force | Out-Null
$cpp = @'
#include <atomic>
#include <cassert>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
thread_local unsigned mockThreadID = 1;
unsigned GetCurrentThreadId() { return mockThreadID; }
namespace RE {
struct Main { unsigned threadID = 1; static inline Main* instance = nullptr; static Main* GetSingleton() { return instance; } };
enum class BSEventNotifyControl { kContinue };
template<class T> struct BSTEventSource {};
template<class T> struct BSTEventSink {
    virtual BSEventNotifyControl ProcessEvent(const T*, BSTEventSource<T>*) = 0;
    virtual ~BSTEventSink() = default;
};
}
namespace SKSE {
// Deliberately no actor, settings, or game-service API in this harness: the
// production sink/queue must compile without any game-state access.
struct EventName { const char* value; const char* c_str() const { return value; } };
struct ModCallbackEvent { EventName eventName; };
struct TaskInterface {
    std::deque<std::function<void()>> pending;
    void AddTask(std::function<void()> task) { pending.push_back(std::move(task)); }
    void Drain() {
        while (!pending.empty()) {
            auto task = std::move(pending.front()); pending.pop_front(); task();
        }
    }
};
TaskInterface* taskInterface = nullptr;
TaskInterface* GetTaskInterface() { return taskInterface; }
namespace log {
unsigned warnings = 0;
void warn(const char*) { ++warnings; }
}
}
bool g_active = false;
std::set<std::string> g_activeSources;
std::string g_previousOutfit;
std::string g_sceneOutfit;
const auto drainThread = std::this_thread::get_id();
std::vector<std::string> calls;
bool checkDrainThread = true;
std::atomic<unsigned> inDispatch{0};
std::function<void()> onDispatch;
void StartScene(const char* source) {
    assert(!checkDrainThread || std::this_thread::get_id() == drainThread);
    assert(inDispatch.fetch_add(1) == 0);
    calls.push_back(std::string("start:") + source);
    if (onDispatch) { auto callback = std::move(onDispatch); onDispatch = {}; callback(); }
    std::this_thread::yield();
    assert(inDispatch.fetch_sub(1) == 1);
}
void EndScene(const char* source) {
    assert(!checkDrainThread || std::this_thread::get_id() == drainThread);
    assert(inDispatch.fetch_add(1) == 0);
    calls.push_back(std::string("end:") + source);
    std::this_thread::yield();
    assert(inDispatch.fetch_sub(1) == 1);
}
'@
$cpp += "
" + $queueAndSink + "
" + $tick + "
" + $resetMatch.Value + "
"
$cpp += @'
void Emit(ModEventSink& sink, const char* name) {
    SKSE::ModCallbackEvent event{{name}};
    assert(sink.ProcessEvent(&event, nullptr) == RE::BSEventNotifyControl::kContinue);
}
void Expect(std::initializer_list<const char*> expected) {
    assert(calls.size() == expected.size());
    unsigned i = 0;
    for (auto entry : expected) { assert(calls[i++] == entry); }
    calls.clear();
}
int main() {
    RE::Main main; RE::Main::instance = &main;
    SKSE::TaskInterface tasks; SKSE::taskInterface = &tasks;
    ModEventSink sink;
    // 1. Null/missing/empty/unknown names must not queue or warn.
    assert(sink.ProcessEvent(nullptr, nullptr) == RE::BSEventNotifyControl::kContinue);
    Emit(sink, nullptr); Emit(sink, ""); Emit(sink, "unrelated");
    assert(tasks.pending.empty() && calls.empty() && SKSE::log::warnings == 0);

    // 2. Worker sender returns without dispatch; an explicit mock drain calls
    // all four in submitted order, with BOTH ends retained for overlap.
    std::thread sender([&] {
        Emit(sink, "ostim_start"); Emit(sink, "PlayerAnimationStart");
        Emit(sink, "ostim_end"); Emit(sink, "PlayerAnimationEnd");
    });
    sender.join();
    assert(calls.empty() && tasks.pending.size() == 1);
    tasks.Drain();
    Expect({"start:OStim", "start:SexLab", "end:OStim", "end:SexLab"});

    // 3. Value ownership: mutate the name, then destroy name/event/sink before drain.
    {
        ModEventSink temporarySink;
        std::string name = "ostim_start";
        SKSE::ModCallbackEvent event{{name.c_str()}};
        temporarySink.ProcessEvent(&event, nullptr);
        name.assign("PlayerAnimationEnd");
        event.eventName.value = nullptr;
    }
    assert(calls.empty() && tasks.pending.size() == 1);
    tasks.Drain(); Expect({"start:OStim"});

    // 4. Repeated ends and starts are not coalesced by the transport.
    Emit(sink, "ostim_start"); Emit(sink, "ostim_end"); Emit(sink, "ostim_end");
    assert(tasks.pending.size() == 1); tasks.Drain();
    Expect({"start:OStim", "end:OStim", "end:OStim"});

    // 5. Execute production Reset: clear existing state, invalidate old queued
    // start AND end, but allow newly submitted events in the same pending FIFO.
    Emit(sink, "ostim_start"); Emit(sink, "PlayerAnimationEnd");
    g_active = true; g_activeSources.insert("OStim");
    g_previousOutfit = "normal"; g_sceneOutfit = "normal_sex";
    const auto generation = g_generation.load();
    Reset();
    assert(g_generation.load() == generation + 1);
    assert(!g_active && g_activeSources.empty() && g_previousOutfit.empty() && g_sceneOutfit.empty());
    Emit(sink, "PlayerAnimationStart"); Emit(sink, "PlayerAnimationEnd");
    assert(tasks.pending.size() == 2); tasks.Drain();
    Expect({"start:SexLab", "end:SexLab"});
    Emit(sink, "ostim_start"); Reset(); Reset(); tasks.Drain(); Expect({});

    // 6. No task interface: warn for every recognized event, drop it; do not
    // run on the sender, retain it for replay, or fall back synchronously.
    SKSE::taskInterface = nullptr;
    std::thread noTasks([&] {
        Emit(sink, "ostim_start"); Emit(sink, "ostim_end");
        Emit(sink, "PlayerAnimationStart"); Emit(sink, "PlayerAnimationEnd");
        Emit(sink, "unrelated");
    });
    noTasks.join();
    assert(SKSE::log::warnings == 4 && calls.empty() && tasks.pending.empty());
    SKSE::taskInterface = &tasks; tasks.Drain(); Expect({});
    Emit(sink, "ostim_start"); tasks.Drain(); Expect({"start:OStim"});

    // 7. Simulate concurrent task-pool Run entry (not a Skyrim guarantee).
    // Even adversarial duplicate drain entry must serialize dispatch and FIFO.
    Emit(sink, "ostim_start"); Emit(sink, "PlayerAnimationStart");
    Emit(sink, "ostim_end"); Emit(sink, "PlayerAnimationEnd");
    auto drain = std::move(tasks.pending.front()); tasks.pending.pop_front();
    checkDrainThread = false;
    std::atomic<bool> go{false};
    auto run = [&] { while (!go.load()) { std::this_thread::yield(); } drain(); };
    std::thread first(run), second(run); go.store(true); first.join(); second.join();
    checkDrainThread = true;
    Expect({"start:OStim", "start:SexLab", "end:OStim", "end:SexLab"});
    assert(!g_drainScheduled && g_pendingEvents.empty());

    // 8. Reentrant event emission during a game-call mock must not deadlock
    // the queue lock; the active drain consumes the newly appended end.
    onDispatch = [&] { Emit(sink, "ostim_end"); };
    Emit(sink, "ostim_start"); tasks.Drain();
    Expect({"start:OStim", "end:OStim"});
    assert(!g_drainScheduled && tasks.pending.empty());

    // 9. Off-main/unknown engine-main identity keeps FIFO without immediate
    // resubmission. Extracted Tick schedules once; only a valid mock ID drains.
    Emit(sink, "ostim_start"); Emit(sink, "ostim_end");
    mockThreadID = 2; tasks.Drain(); Expect({});
    assert(!g_drainScheduled && tasks.pending.empty() && g_pendingEvents.size() == 2);
    Tick(); Tick(); assert(tasks.pending.size() == 1);
    tasks.Drain(); Expect({}); assert(tasks.pending.empty());
    mockThreadID = 1; RE::Main::instance = nullptr;
    Tick(); tasks.Drain(); Expect({}); assert(g_pendingEvents.size() == 2);
    RE::Main::instance = &main; main.threadID = 0;
    Tick(); tasks.Drain(); Expect({}); assert(g_pendingEvents.size() == 2);
    main.threadID = 1; Tick(); Tick(); assert(tasks.pending.size() == 1);
    tasks.Drain(); Expect({"start:OStim", "end:OStim"});
    Tick(); assert(tasks.pending.empty());
    Emit(sink, "ostim_start"); mockThreadID = 2; tasks.Drain();
    Reset(); mockThreadID = 1; Tick(); assert(tasks.pending.empty());
}
'@
Set-Content -LiteralPath (Join-Path $work 'test_scene_queue.cpp') -Value $cpp -Encoding utf8
$cmd = 'call "' + $VcVars + '" >nul && cl /nologo /EHsc /W4 /std:c++20 test_scene_queue.cpp /Fe:test_scene_queue.exe && test_scene_queue.exe'
Push-Location $work
try {
    cmd /c $cmd
    if ($LASTEXITCODE -ne 0) { throw 'Scene outfit queue regression failed' }
} finally { Pop-Location }
Write-Output 'SCENE_OUTFIT_QUEUE_REGRESSION_OK: 9 groups; production dispatch/queue/sink/Tick/Reset extracted verbatim; owned FIFO with mocked task execution only; not an engine test or proof of main-thread execution.'
