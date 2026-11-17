$ErrorActionPreference = 'Stop'
# This extracts production control flow, not a live SKSE or engine integration test.
$work = Join-Path $PSScriptRoot '../build/morph-interface-regression'
New-Item -ItemType Directory -Path $work -Force | Out-Null
$source = Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/MorphRefresh.cpp')
function Get-ProductionRegion([string] $begin, [string] $end) {
    $start = $source.IndexOf($begin, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Production marker missing: $begin" }
    $finish = $source.IndexOf($end, $start, [StringComparison]::Ordinal)
    if ($finish -le $start) { throw "Production end marker missing: $end" }
    return $source.Substring($start, $finish - $start)
}
$tryRaceMenu = Get-ProductionRegion 'bool TryRaceMenu(' 'bool TryOBody('
$probe = Get-ProductionRegion 'void ProbeRaceMenuInterface()' 'bool Available()'
$cpp = @'
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
// Deliberately minimal doubles: only queried ABI prefix and exchange layout.
// This does not validate the full BodyMorph vtable against the shipped DLL.
namespace skee {
class IPluginInterface {
public:
    virtual ~IPluginInterface() = default;
    virtual std::uint32_t GetVersion() = 0;
    virtual void Revert() = 0;
};
class IBodyMorphInterface : public IPluginInterface {};
class IInterfaceMap {
public:
    virtual IPluginInterface* QueryInterface(const char*) = 0;
    virtual bool AddInterface(const char*, IPluginInterface*) = 0;
    virtual IPluginInterface* RemoveInterface(const char*) = 0;
};
struct InterfaceExchangeMessage {
    enum { kMessage_ExchangeInterface = 0x9E3779B9 };
    IInterfaceMap* interfaceMap = nullptr;
};
}
static_assert(sizeof(void*) == 8, "Run this test with the x64 compiler");
static_assert(sizeof(skee::InterfaceExchangeMessage) == 8);
struct Candidate final : skee::IBodyMorphInterface {
    std::uint32_t version = 4;
    unsigned versionCalls = 0;
    std::uint32_t GetVersion() override { ++versionCalls; return version; }
    void Revert() override { assert(false); }
};
struct Map final : skee::IInterfaceMap {
    skee::IPluginInterface* candidate = nullptr;
    unsigned queries = 0;
    skee::IPluginInterface* QueryInterface(const char* name) override {
        assert(std::strcmp(name, "BodyMorph") == 0); ++queries; return candidate;
    }
    bool AddInterface(const char*, skee::IPluginInterface*) override { assert(false); return false; }
    skee::IPluginInterface* RemoveInterface(const char*) override { assert(false); return nullptr; }
};
namespace SKSE {
namespace log {
    template<class... Args> void info(const char*, Args&&...) {}
    template<class... Args> void warn(const char*, Args&&...) {}
}
unsigned GetPluginHandle() { return 42; }
struct MessagingInterface {
    struct Reply { std::string receiver; bool delivered; skee::IInterfaceMap* map; };
    std::vector<Reply> replies;
    mutable unsigned calls = 0;
    bool Dispatch(std::uint32_t type, void* data, std::uint32_t size, const char* receiver) const {
        assert(type == 0x9E3779B9u && size == 8 && data);
        assert(calls < replies.size());
        const auto& reply = replies[calls++];
        assert(reply.receiver == (receiver ? receiver : "(broadcast)"));
        auto* exchange = static_cast<skee::InterfaceExchangeMessage*>(data);
        assert(exchange->interfaceMap == nullptr);
        if (reply.map) { assert(reply.delivered); exchange->interfaceMap = reply.map; }
        return reply.delivered;
    }
};
MessagingInterface* messaging = nullptr;
const MessagingInterface* GetMessagingInterface() { return messaging; }
}
skee::IBodyMorphInterface* g_api = nullptr;
std::uint32_t g_apiVersion = 0;
'@
$cpp += $tryRaceMenu
$cpp += $probe
$cpp += @'
int main() {
    Candidate candidate;
    Map map; map.candidate = &candidate;
    SKSE::MessagingInterface messaging;
    SKSE::messaging = &messaging;
    auto reset = [&] {
        g_api = nullptr; g_apiVersion = 0;
        messaging.calls = 0; messaging.replies.clear();
        map.queries = 0; map.candidate = &candidate;
        candidate.version = 4; candidate.versionCalls = 0;
    };
    // 1: No callback delivery at any receiver.
    reset(); messaging.replies = {{"skee64", false, nullptr}, {"skee", false, nullptr}, {"(broadcast)", false, nullptr}};
    assert(!TryRaceMenu(&messaging)); assert(messaging.calls == 3 && !g_api && map.queries == 0);
    // 2: Delivery alone must not imply an interface response.
    reset(); messaging.replies = {{"skee64", true, nullptr}, {"skee", true, nullptr}, {"(broadcast)", true, nullptr}};
    ProbeRaceMenuInterface(); assert(messaging.calls == 3 && !g_api && g_apiVersion == 0);
    // 3: Exact OBody receiver path, valid v4; stop before broadcast.
    reset(); messaging.replies = {{"skee64", false, nullptr}, {"skee", true, &map}};
    assert(TryRaceMenu(&messaging));
    assert(messaging.calls == 2 && g_api == &candidate && g_apiVersion == 4);
    assert(map.queries == 1 && candidate.versionCalls == 1);
    // 4: A returned map with v3 is not accepted.
    reset(); candidate.version = 3; messaging.replies = {{"skee64", true, &map}};
    assert(!TryRaceMenu(&messaging)); assert(!g_api && g_apiVersion == 0 && messaging.calls == 1);
    // 5: Broadcast delivered only to an unrelated listener, no map response.
    reset(); messaging.replies = {{"skee64", false, nullptr}, {"skee", false, nullptr}, {"(broadcast)", true, nullptr}};
    assert(!TryRaceMenu(&messaging)); assert(!g_api && messaging.calls == 3 && map.queries == 0);
    // 6: The public probe must not dispatch again after acquisition.
    reset(); messaging.replies = {{"skee64", true, &map}};
    ProbeRaceMenuInterface(); ProbeRaceMenuInterface();
    assert(g_api == &candidate && messaging.calls == 1 && candidate.versionCalls == 1);
    // 7: No messaging interface is a safe no-op.
    reset(); SKSE::messaging = nullptr; ProbeRaceMenuInterface();
    assert(!g_api && g_apiVersion == 0 && messaging.calls == 0);
    SKSE::messaging = &messaging;
    // 8: Map returned but BodyMorph entry missing.
    reset(); map.candidate = nullptr; messaging.replies = {{"skee64", true, &map}};
    assert(!TryRaceMenu(&messaging)); assert(!g_api && map.queries == 1 && candidate.versionCalls == 0);
    // 9: An actual map response to broadcast may be accepted.
    reset(); messaging.replies = {{"skee64", false, nullptr}, {"skee", false, nullptr}, {"(broadcast)", true, &map}};
    assert(TryRaceMenu(&messaging)); assert(g_api == &candidate && g_apiVersion == 4 && messaging.calls == 3);
    // 10: Quiet retry follows the same acceptance rules.
    reset(); messaging.replies = {{"skee64", true, &map}};
    assert(TryRaceMenu(&messaging, true)); assert(g_api == &candidate && g_apiVersion == 4);
}
'@
Set-Content -Path (Join-Path $work 'test_interface.cpp') -Value $cpp -Encoding utf8
$vcvars = 'C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
$cmd = 'call "' + $vcvars + '" >nul && cl /nologo /EHsc /std:c++20 test_interface.cpp /Fe:test_interface.exe && test_interface.exe'
Push-Location $work
try { cmd /c $cmd; if ($LASTEXITCODE -ne 0) { throw 'Morph interface mock regression failed' } } finally { Pop-Location }
Write-Output 'MORPH_INTERFACE_MOCK_OK: 10 scenarios; production TryRaceMenu and ProbeRaceMenuInterface extracted verbatim; minimal ABI/control-flow doubles, not real SKSE routing, shipped DLL ABI, or engine validation'
