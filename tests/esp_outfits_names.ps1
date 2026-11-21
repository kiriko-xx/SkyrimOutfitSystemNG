$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$work = Join-Path $root 'build/esp_outfits_match_test'
New-Item -ItemType Directory -Path $work -Force | Out-Null
$vcvars = 'C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
$cmd = 'call "' + $vcvars + '" >nul && cl /nologo /W4 /WX /EHsc /utf-8 /std:c++20 /I"' + $root + '/src" "' + $PSScriptRoot + '/esp_outfits_match.cpp" /Fe:esp_outfits_match.exe && esp_outfits_match.exe'
Push-Location $work
try { cmd /c $cmd; if ($LASTEXITCODE -ne 0) { throw 'ESP outfit matching regression failed' } } finally { Pop-Location }
