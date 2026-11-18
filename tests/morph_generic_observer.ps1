$ErrorActionPreference='Stop'
$s=Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/MorphRefresh.cpp')
$a=$s.IndexOf('} else if (g_api && providerSignature != state.actedProvider)')
$b=$s.IndexOf('} else if (state.value != signature)', $a)
if($a -lt 0 -or $b -le $a){throw 'Generic provider change branch missing'}
$branch=$s.Substring($a+2,$b-$a-2)+'}'
$a=$s.IndexOf('EnsureProvidersForUse();', $b)
$a=$s.IndexOf('if (g_api) {',$a)
$b=$s.IndexOf('if (Settings::Get().morphProviderFirst', $a)
$replay=$s.Substring($a,$b-$a)
if($replay.Contains('IsMenuOpen') -or -not $replay.Contains('EnqueueBake(id, BakeReason::kProvider)')){throw 'Replay must be menu-independent'}
$tick=$s.Substring($s.IndexOf('void RunPollBody('))
$poll=$tick.IndexOf('PollOnMainThread(a_mainThread);')
$drain=$tick.IndexOf('DrainPending();')
if($poll -lt 0 -or $drain -le $poll -or $replay.Contains('AddTask') -or $replay.Contains('RequestBake(')){throw 'Observer must enqueue, then drain in the same accepted poll'}
$cpp=@'
#include <atomic>
#include <cassert>
struct { std::atomic<unsigned> changes{}; } g_pollTrace;
#include <cstdint>
struct State { std::uint64_t actedProvider=1; };
void* g_api=nullptr;
namespace SKSE::log { template<class... T> void debug(T...){} }
enum class BakeReason{kProvider};
unsigned requests=0;
void EnqueueBake(unsigned id,BakeReason){assert(id==0x14);++requests;}
bool changedValue(State& state,std::uint64_t providerSignature){bool changed=false;
if(false){}
'@
$cpp+=$branch+"
return changed;}
void replay(){unsigned id=0x14;for(int once=0;once<1;++once){"+$replay+"}}
"
$cpp+=@'
int main(){
 State state; int api=0;
 assert(!changedValue(state,2)); assert(state.actedProvider==1);
 g_api=&api;
 assert(!changedValue(state,1));
 assert(changedValue(state,2)); assert(state.actedProvider==2);
 assert(!changedValue(state,2));
 for(unsigned i=3;i<103;++i){assert(changedValue(state,i)); replay();}
 assert(requests==100); // Continuous ramps need no signature settle or quiet expiry.
 assert(changedValue(state,1)); replay(); assert(requests==101); // Return to original.
 g_api=nullptr; replay(); assert(requests==101);
 assert(g_pollTrace.changes.load()==101);
}
'@
$w=Join-Path $PSScriptRoot '../build/morph-generic-regression'
New-Item -ItemType Directory -Force $w | Out-Null
Set-Content (Join-Path $w 'test_generic.cpp') $cpp -Encoding utf8
$vc='C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
Push-Location $w
try{cmd /c ('call "'+$vc+'" >nul && cl /nologo /EHsc /std:c++20 test_generic.cpp /Fe:test_generic.exe && test_generic.exe');if($LASTEXITCODE -ne 0){throw 'Generic observer regression failed'}}finally{Pop-Location}
Write-Output 'GENERIC_OBSERVER_OK: extracted provider-change/replay branches, continuous ramp, return-to-baseline, no menu gate; mock not engine test'
