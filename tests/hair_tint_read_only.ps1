$ErrorActionPreference = 'Stop'
$src = Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/HairTintDiagnostics.cpp')
$morph = Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/MorphRefresh.cpp')
foreach ($required in @('kMaxRoots = 8', 'kMaxObjects = 512', 'kMaxGeometries = 64', 'netimmerse_cast<RE::BSLightingShaderProperty*>', 'BSShaderMaterial::Type::kLighting', 'BSShaderMaterial::Feature::kHairTint', 'spdlog::should_log(spdlog::level::debug)', 'g_lastSample', 'std::atomic<unsigned> g_events', 'GetHeadPartObject(RE::BGSHeadPart::HeadPartType::kHair)', 'source=headpart-hair', 'source=illusion-slots')) {
 if (-not $src.Contains($required)) { throw "Missing diagnostic guard: $required" }
}
$begin = $src.IndexOf('class MenuSink')
$end = $src.IndexOf('std::uintptr_t Identity', $begin)
$sinks = $src.Substring($begin, $end - $begin)
foreach ($forbidden in @('GetSingleton', 'AddTask', 'SKSE::log', 'RequestBake', 'g_api', 'tintColor')) {
 if ($sinks.Contains($forbidden)) { throw "Callback must only signal: $forbidden" }
}
foreach ($forbidden in @('SetHairColor', 'UpdateHairColor', 'ApplyVertexDiff', 'ApplyBodyMorphs', 'QueueNiNodeUpdate', 'SetVertex', 'memcpy(', 'memset(')) {
 if ($src.Contains($forbidden)) { throw "Unexpected write operation: $forbidden" }
}
# The main-thread task that samples the skeleton is the same one that retries the
# providers and drains the sliders; find it from the sample call itself (the lambda
# captures nothing, so its header is not a stable anchor).
$sample = $morph.IndexOf('HairTintDiagnostics::SampleOnMainThread(')
$task = $morph.LastIndexOf('tasks->AddTask(', $sample)
$retry = $morph.IndexOf('RetryProviders();', $task)
$drain = $morph.IndexOf('DrainRaceMenuSlider();', $task)
if ($sample -lt 0 -or $task -lt 0 -or $sample -le $task -or $sample -ge $retry -or $sample -ge $drain) { throw 'Sampling must precede provider and morph writes in main-thread task' }
Write-Output 'HAIR_TINT_READ_ONLY_GUARDS_OK: bounded, type-guarded, queue-only callbacks; static checks, not engine validation'
