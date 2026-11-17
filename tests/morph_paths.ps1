$ErrorActionPreference = 'Stop'
$work = Join-Path $PSScriptRoot '../build/morph-path-regression'
New-Item -ItemType Directory -Path $work -Force | Out-Null
$source = Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/MorphRefresh.cpp')
$start = $source.IndexOf('std::string MeshRelativePath(')
$end = $source.IndexOf('bool TriFileExists(', $start)
$func = $source.Substring($start, $end - $start)
$cpp = "#include <algorithm>
#include <cctype>
#include <string>
#include <cassert>
" + $func + @'
int main() {
assert(MeshRelativePath("Meshes/mgyma/Muguangyuming_1.nif") == "mgyma\\muguangyuming_1.nif");
assert(MeshRelativePath("meshes\\meshes\\A.tri") == "a.tri");
assert(MeshRelativePath("actors\\character\\body.tri") == "actors\\character\\body.tri");
assert(MeshRelativePath("../outside.tri").empty());
assert(MeshRelativePath("C:/outside.tri").empty());
assert(MeshRelativePath("").empty());
}
'@
Set-Content -Path (Join-Path $work 'test_morph_paths.cpp') -Value $cpp -Encoding utf8
$vcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
$cmd = 'call "' + $vcvars + '" >nul && cl /nologo /EHsc /std:c++20 test_morph_paths.cpp /Fe:test_morph_paths.exe && test_morph_paths.exe'
Push-Location $work
try { cmd /c $cmd; if ($LASTEXITCODE -ne 0) { throw 'Path regression failed' } } finally { Pop-Location }
Write-Output 'PATH_REGRESSION_OK: 6 cases, production function extracted verbatim'