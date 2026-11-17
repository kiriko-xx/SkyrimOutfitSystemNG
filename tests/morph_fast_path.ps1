$ErrorActionPreference='Stop'
$s=Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/MorphRefresh.cpp')
$h=Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/HairTintDiagnostics.cpp')
foreach($t in @('kPollInterval = std::chrono::milliseconds(33)', 'const auto signature = g_api ? providerSignature : ComputeSignature(actor);','const auto baseline = g_api ? providerSignature : ComputeSignature(a_actor);','g_api && replayed.insert(node).second')){if(-not $s.Contains($t)){throw "Missing fast path: $t"}}
$a=$s.IndexOf('std::uint64_t ComputeSignature(');$b=$s.IndexOf('void ApplyMorphs(',$a);$f=$s.Substring($a,$b-$a)
if($f.IndexOf('if (g_api)') -ge $f.IndexOf('ComputeGeometrySignature(a_actor)')){throw 'Authoritative API must bypass expensive vertex hashing'}
if(-not $h.Contains('now - g_lastSampleTime < std::chrono::milliseconds(250)')){throw 'Hair diagnostics must remain slow'}
if($h.IndexOf('now - g_lastSampleTime') -ge $h.IndexOf('const auto events = g_events.exchange')){throw 'Throttle must preserve pending events'}
Write-Output 'FAST_PATH_GUARDS_OK: 33ms cadence, authoritative signature reuse, node dedup, 250ms diagnostics; static not runtime perf proof'
