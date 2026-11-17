$ErrorActionPreference = 'Stop'
$s = Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/MorphRefresh.cpp')
$a = $s.IndexOf('bool IsGameThread()')
$b = $s.IndexOf('// Minimal ABI', $a)
if ($a -lt 0 -or $b -le $a) { throw 'Missing production thread predicate' }
$f = $s.Substring($a, $b-$a)
foreach ($marker in @('morphPush::g_taskQueued.store(false);', 'tasks->AddTask([retryProviders]()')) {
 $i=$s.IndexOf($marker); if($i -lt 0){throw 'Missing task entry'}
 if(-not $s.Substring($i,[Math]::Min(300,$s.Length-$i)).Contains('if (!IsGameThread())')) {throw 'Task entry lacks thread rejection'}
}
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
Write-Output 'MORPH_THREAD_GATE_OK: production predicate 5 cases and task-entry static guards; not engine phase proof'
