$ErrorActionPreference = 'Stop'
$work = Join-Path $PSScriptRoot '../build/morph-slider-regression'
New-Item -ItemType Directory -Path $work -Force | Out-Null
$source = Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/MorphRefresh.cpp')
$start = $source.IndexOf('std::atomic<bool> g_raceMenuSliderPending')
$end = $source.IndexOf('class OBodyMorphSink', $start)
if ($start -lt 0 -or $end -le $start) { throw 'Production slider drain not found' }
$func = $source.Substring($start, $end - $start)
$cpp = @'
#include <atomic>
#include <cassert>
#include <thread>
namespace RE {
struct UI { static inline UI* instance=nullptr; bool open=true; static UI* GetSingleton(){return instance;} bool IsMenuOpen(const char*){return open;} };
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
 RE::UI ui; RE::UI::instance=&ui;
 RE::PlayerCharacter player; RE::PlayerCharacter::instance=&player;
 g_api=&ui;
 DrainRaceMenuSlider(); assert(calls==0);
 auto signal=[] { g_raceMenuSliderPending.store(true,std::memory_order_release); };
 signal(); signal(); DrainRaceMenuSlider(); assert(calls==1); DrainRaceMenuSlider(); assert(calls==1);
 g_api=nullptr; signal(); DrainRaceMenuSlider(); assert(calls==1); g_api=&ui; DrainRaceMenuSlider(); assert(calls==1);
 ui.open=false; signal(); DrainRaceMenuSlider(); assert(calls==1); ui.open=true;
 RE::UI::instance=nullptr; signal(); DrainRaceMenuSlider(); assert(calls==1); RE::UI::instance=&ui;
 player.loaded=false; signal(); DrainRaceMenuSlider(); assert(calls==1); player.loaded=true;
 RE::PlayerCharacter::instance=nullptr; signal(); DrainRaceMenuSlider(); assert(calls==1); RE::PlayerCharacter::instance=&player;
 std::thread producer([&]{for(int i=0;i<10000;++i)signal();}); producer.join(); DrainRaceMenuSlider(); assert(calls==2); DrainRaceMenuSlider(); assert(calls==2);
 signal(); g_raceMenuSliderPending.store(false,std::memory_order_release); DrainRaceMenuSlider(); assert(calls==2);
}
'@
Set-Content -Path (Join-Path $work 'test_slider.cpp') -Value $cpp -Encoding utf8
$vcvars = 'C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
$cmd = 'call "' + $vcvars + '" >nul && cl /nologo /EHsc /std:c++20 test_slider.cpp /Fe:test_slider.exe && test_slider.exe'
Push-Location $work
try { cmd /c $cmd; if ($LASTEXITCODE -ne 0) { throw 'Slider drain regression failed' } } finally { Pop-Location }
Write-Output 'SLIDER_QUEUE_REGRESSION_OK: 9 scenarios, production drain extracted verbatim; mocked UI and actor, not an engine test'
