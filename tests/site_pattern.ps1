$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$work = Join-Path $root 'build/site_pattern_test'
New-Item -ItemType Directory -Path $work -Force | Out-Null
$vcvars = 'C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
$cmd = 'call "' + $vcvars + '" >nul && cl /nologo /W4 /WX /wd4505 /EHsc /utf-8 /std:c++20 /I"' + $root + '/src/cpp/include" "' + $PSScriptRoot + '/site_pattern.cpp" /Fe:site_pattern.exe && site_pattern.exe'
Push-Location $work
try { cmd /c $cmd; if ($LASTEXITCODE -ne 0) { throw 'Site fingerprint regression failed' } } finally { Pop-Location }
