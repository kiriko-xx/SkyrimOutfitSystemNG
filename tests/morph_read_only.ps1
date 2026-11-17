$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/MorphRefresh.cpp')
$forbidden = @('->ApplyOBodyMorphsToActor(', '->RemoveOBodyMorphsFromActor(', '->ForcefullyChangeORefitForActor(', 'ApplyOBodyMorphs(actor)')
foreach ($token in $forbidden) {
    if ($source.Contains($token)) { throw ('Automatic morph path contains preset writer: ' + $token) }
}
if ($source.Contains('set it to 0 and restart')) { throw 'Unsupported INI advice remains' }
if (-not $source.Contains('return g_api != nullptr;')) { throw 'Mesh-only provider gate missing' }
$start = $source.IndexOf('if (a_event && a_event->eventName == "RSM_SliderChange")')
if ($start -lt 0) { throw 'Public RaceMenu slider event missing' }
$end = $source.IndexOf('if (a_event && a_event->sender', $start)
$eventBlock = $source.Substring($start, $end - $start)
if (-not $eventBlock.Contains('g_raceMenuSliderPending.store(true, std::memory_order_release);')) { throw 'Atomic event handoff missing' }
foreach ($token in @('g_api', 'GetSingleton', 'RequestBake(')) {
    if ($eventBlock.Contains($token)) { throw ('Game access in callback: ' + $token) }
}
if (-not $source.Contains('DrainRaceMenuSlider();')) { throw 'Main-thread event drain missing' }
foreach ($token in @('ApplyVertexDiff(', 'UpdateHairColor(', 'ApplyBodyMorphs(')) {
    if ($eventBlock.Contains($token)) { throw ('Synchronous mutation in slider callback: ' + $token) }
}
Write-Output 'MORPH_READ_ONLY_GUARDS_OK: no preset writers; public slider event is interface-gated and queue-only'
