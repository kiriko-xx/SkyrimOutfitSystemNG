$ErrorActionPreference = 'Stop'
# 1.5.36: the poll no longer asks who the thread is. 1.5.35 gated every poll on the
# game thread id and the reporting machine rejected 640 of 1241 polls - in runs that
# sat exactly on the seconds a body morph slider was dragged - while the engine's own
# worn passes ran on those same worker threads. The poll now runs wherever it is
# asked from, and the bake is only allowed off the main thread from inside the
# engine's own worn pass (SkinTick).
$s = Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/MorphRefresh.cpp')
$a = $s.IndexOf('bool OnGameThread()')
$b = $s.IndexOf('// Minimal ABI', $a)
if ($a -lt 0 -or $b -le $a) { throw 'Missing production thread predicate' }
$f = $s.Substring($a, $b-$a)

if ($s.Contains('IsGameThread')) { throw 'The poll is still gated on the game thread identity' }
foreach ($leftover in @('g_pollTrace.rejected', 'wrong-thread={')) { if ($s.Contains($leftover)) { throw "The rejected-as-wrong-thread path is still there: $leftover" } }

# Two poll sources, one throttle, one entry point each.
$tick = $s.IndexOf('void Tick()')
if ($tick -lt 0) { throw 'Missing timer tick' }
$tickWindow = $s.Substring($tick, [Math]::Min(1600, $s.Length-$tick))
if (-not $tickWindow.Contains('PollOnce(false)')) { throw 'The timer no longer polls' }
if ($tickWindow.Contains('AddTask')) { throw 'The timer still posts a task; that is the path that was rejected' }

$skin = $s.IndexOf('void SkinTick()')
if ($skin -lt 0) { throw 'Missing engine-side anchor entry point' }
$skinWindow = $s.Substring($skin, [Math]::Min(400, $s.Length-$skin))
if (-not $skinWindow.Contains('PollOnce(true)')) { throw 'The engine anchor does not poll with baking allowed' }

$poll = $s.IndexOf('void PollOnce(bool a_allowBake)')
if ($poll -lt 0) { throw 'Missing shared poll entry point' }
$pollBody = $s.Substring($poll, [Math]::Min(2200, $s.Length-$poll))
foreach ($needle in @('ClaimPollSlot()', 'RunPollBody(mainThread, mainThread || a_allowBake)', 'reentry.exchange(true')) {
 if (-not $pollBody.Contains($needle)) { throw "Poll entry point lacks $needle" }
}

# The engine-side anchor is the skinning hook, and it has to be a real call site.
$patches = Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/hooking/Patches.cpp')
if (-not $patches.Contains('SOS::MorphRefresh::SkinTick();')) { throw 'The skinning pass does not drive the poll' }
$overrideStart = $patches.IndexOf('bool ShouldOverrideSkinning(')
$overrideEnd = $patches.IndexOf('class EquippedArmorVisitor', $overrideStart)
if ($overrideStart -lt 0 -or $overrideEnd -le $overrideStart) { throw 'Missing skinning entry point' }
if (-not $patches.Substring($overrideStart, $overrideEnd-$overrideStart).Contains('SkinTick()')) { throw 'The anchor is not inside the skinning entry point' }

$hooks = Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/hooking/Hooks.cpp')
if ($hooks.Contains('FramePump') -or $hooks.Contains('FrameTick')) { throw 'The dead per-frame pump is still installed' }
$main = Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/main.cpp')
if ($main.Contains('ApplyFramePumpHook')) { throw 'The dead per-frame pump is still installed' }

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
 assert(!OnGameThread());
 RE::Main main; RE::Main::instance=&main;
 assert(!OnGameThread());
 main.threadID=42; assert(!OnGameThread());
 mockCurrent=42; assert(OnGameThread());
 mockCurrent=43; assert(!OnGameThread());
}
'@
Set-Content (Join-Path $work 'gate.cpp') $cpp -Encoding utf8
$vc='C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
Push-Location $work
try { cmd /c ('call "'+$vc+'" >nul && cl /nologo /EHsc /std:c++20 /W4 /WX /UNDEBUG gate.cpp /Fe:gate.exe && gate.exe'); if($LASTEXITCODE -ne 0){throw 'Thread predicate regression failed'} } finally { Pop-Location }
Write-Output 'MORPH_THREAD_GATE_OK: predicate 5 cases; the poll runs wherever it is asked and is not gated on thread identity; the bake is allowed off the main thread only from the engine worn pass; the dead per-frame pump is gone'
