$ErrorActionPreference = 'Stop'
$work = Join-Path $PSScriptRoot '../build/morph-slider-regression'
New-Item -ItemType Directory -Path $work -Force | Out-Null
$source = Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/MorphRefresh.cpp')
$start = $source.IndexOf('std::atomic<bool> g_raceMenuSliderPending')
$end = $source.IndexOf('class OBodyMorphSink', $start)
if ($start -lt 0 -or $end -le $start) { throw 'Production slider drain not found' }
$func = $source.Substring($start, $end - $start)
# 1.5.35: the event is the signal. A menu-name gate is what made this path dead on
# the reporting machine (the slider was dragged from Screen Archer Menu, so
# "RaceSex Menu" was never open), so the drain must not look at any menu again.
if ($func -match 'IsMenuOpen') { throw 'The slider drain must not gate on a menu name' }
if ($func -match 'RE::UI') { throw 'The slider drain must not consult the UI singletons' }
if (-not $func.Contains('if (!g_api)')) { throw 'The slider drain must still require the interface' }
$cpp = @'
#include <atomic>
#include <cassert>
#include <thread>
namespace RE {
struct PlayerCharacter { static inline PlayerCharacter* instance=nullptr; bool loaded=true; static PlayerCharacter* GetSingleton(){return instance;} bool Is3DLoaded(){return loaded;} unsigned GetFormID(){return 0x14;} };
}
void* g_api=nullptr;
enum class BakeReason { kProvider };
unsigned calls=0;
void RequestBake(unsigned id, BakeReason reason) { assert(id==0x14); assert(reason==BakeReason::kProvider); ++calls; }
'@
$cpp += $func
$cpp += @'
int main() {
 RE::PlayerCharacter player; RE::PlayerCharacter::instance=&player;
 g_api=&player;
 DrainRaceMenuSlider(); assert(calls==0);
 auto signal=[] { g_raceMenuSliderPending.store(true,std::memory_order_release); };
 signal(); signal(); DrainRaceMenuSlider(); assert(calls==1); DrainRaceMenuSlider(); assert(calls==1);
 g_api=nullptr; signal(); DrainRaceMenuSlider(); assert(calls==1); g_api=&player; DrainRaceMenuSlider(); assert(calls==1);
 player.loaded=false; signal(); DrainRaceMenuSlider(); assert(calls==1); player.loaded=true;
 RE::PlayerCharacter::instance=nullptr; signal(); DrainRaceMenuSlider(); assert(calls==1); RE::PlayerCharacter::instance=&player;
 std::thread producer([&]{for(int i=0;i<10000;++i)signal();}); producer.join(); DrainRaceMenuSlider(); assert(calls==2); DrainRaceMenuSlider(); assert(calls==2);
 signal(); g_raceMenuSliderPending.store(false,std::memory_order_release); DrainRaceMenuSlider(); assert(calls==2);
 signal(); DrainRaceMenuSlider(); assert(calls==3);
}
'@
Set-Content -Path (Join-Path $work 'test_slider.cpp') -Value $cpp -Encoding utf8
$vcvars = 'C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
$cmd = 'call "' + $vcvars + '" >nul && cl /nologo /EHsc /std:c++20 test_slider.cpp /Fe:test_slider.exe && test_slider.exe'
Push-Location $work
try { cmd /c $cmd; if ($LASTEXITCODE -ne 0) { throw 'Slider drain regression failed' } } finally { Pop-Location }
Write-Output 'SLIDER_QUEUE_REGRESSION_OK: 8 scenarios, production drain extracted verbatim; no menu-name gate, atomic hand-off only; mocked actor, not an engine test'
