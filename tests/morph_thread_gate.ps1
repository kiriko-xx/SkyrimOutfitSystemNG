$ErrorActionPreference = 'Stop'
$s = Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/MorphRefresh.cpp')
$a = $s.IndexOf('bool IsGameThread()')
$b = $s.IndexOf('// Minimal ABI', $a)
if ($a -lt 0 -or $b -le $a) { throw 'Missing production thread predicate' }
$f = $s.Substring($a, $b-$a)

# The fallback task entry still rejects the wrong thread. Since 1.5.35 it is no longer
# the primary path: the per-frame pump (FrameTick) is, and that one cannot be rejected.
$start = $s.IndexOf('morphPush::g_taskQueued.store(false);')
if ($start -lt 0) { throw 'Missing task entry' }
if (-not $s.Substring($start,[Math]::Min(450,$s.Length-$start)).Contains('!IsGameThread()')) { throw 'Task entry lacks thread rejection' }

$tick = $s.IndexOf("void Tick()")
if ($tick -lt 0) { throw 'Missing fallback tick' }
$tickTask = $s.IndexOf('tasks->AddTask([]() {', $tick)
if ($tickTask -lt 0) { throw 'Missing fallback task' }
$tickWindow = $s.Substring($tickTask,[Math]::Min(900,$s.Length-$tickTask))
if (-not $tickWindow.Contains('!IsGameThread()')) { throw 'Fallback task lacks thread rejection' }
if (-not $tickWindow.Contains('ClaimPollSlot()')) { throw 'Fallback task must share the poll slot with the pump' }
if (-not $s.Substring($tick,[Math]::Min(1200,$s.Length-$tick)).Contains('g_lastPost')) { throw 'Fallback posting must be throttled' }

# The pump, and the anchor it is installed on.
$pump = $s.IndexOf('void FrameTick()')
if ($pump -lt 0) { throw 'Missing per-frame pump' }
$pumpBody = $s.Substring($pump,[Math]::Min(2600,$s.Length-$pump))
foreach ($needle in @('ClaimPollSlot()', 'RunPollBody()', 'the pump leaves the poll to the timer')) {
 if (-not $pumpBody.Contains($needle)) { throw "Per-frame pump lacks $needle" }
}
$hooks = Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/hooking/Hooks.cpp')
if (-not $hooks.Contains('SOS::MorphRefresh::FrameTick')) { throw 'The anchor does not call the pump' }
if (-not $hooks.Contains('REL::RelocationID MainUpdate(')) { throw 'The anchor lost its address library entry' }
if (-not $hooks.Contains('FindCallsInModule("FramePump"')) { throw 'The anchor lost its call-site scan' }
$main = Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/main.cpp')
if (-not $main.Contains('Hooking::ApplyFramePumpHook()')) { throw 'The pump is not installed' }

$work=Join-Path $PSScriptRoot '../build/morph-thread-regression'
New-Item -ItemType Directory -Force $work | Out-Null
$cpp=@'
#include <cassert>
#include <cstdint>
#include <atomic>
namespace SKSE::log { template<class... T> void info(const char*, T...){} }
namespace RE { struct Main { static inline Main* instance=nullptr; std::uint32_t threadID=0; static Main* GetSingleton(){return instance;} }; }
std::uint32_t mockCurrent=99;
std::uint32_t GetCurrentThreadId(){return mockCurrent;}
'@
$cpp += $f
$cpp += @'
int main(){
 assert(!IsGameThread());
 RE::Main main; RE::Main::instance=&main;
 assert(!IsGameThread());
 main.threadID=42; assert(!IsGameThread());
 mockCurrent=42; assert(IsGameThread());
 mockCurrent=43; assert(!IsGameThread());
}
'@
Set-Content (Join-Path $work 'gate.cpp') $cpp -Encoding utf8
$vc='C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
Push-Location $work
try { cmd /c ('call "'+$vc+'" >nul && cl /nologo /EHsc /std:c++20 /W4 /WX /UNDEBUG gate.cpp /Fe:gate.exe && gate.exe'); if($LASTEXITCODE -ne 0){throw 'Thread predicate regression failed'} } finally { Pop-Location }
Write-Output 'MORPH_THREAD_GATE_OK: production predicate 5 cases; fallback task still rejects the wrong thread and shares the poll slot; per-frame pump installed on the Main::Update call site; not engine phase proof'
