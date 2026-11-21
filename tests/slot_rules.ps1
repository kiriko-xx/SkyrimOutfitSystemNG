$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$work = Join-Path $root 'build/slot_rules_test'
New-Item -ItemType Directory -Path $work -Force | Out-Null
$vcvars = 'C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
$cmd = 'call "' + $vcvars + '" >nul && cl /nologo /W4 /WX /wd4505 /EHsc /utf-8 /std:c++20 /I"' + $root + '/src" "' + $PSScriptRoot + '/slot_rules.cpp" /Fe:slot_rules.exe && slot_rules.exe'
Push-Location $work
try { cmd /c $cmd; if ($LASTEXITCODE -ne 0) { throw 'Slot rule regression failed' } } finally { Pop-Location }
