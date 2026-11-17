$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$source = Get-Content -Raw (Join-Path $root 'src/cpp/MorphRefresh.cpp')
$main = Get-Content -Raw (Join-Path $root 'src/cpp/main.cpp')
$start = $source.IndexOf('void ProbeRaceMenuInterface()')
$end = $source.IndexOf('bool Available()', $start)
if ($start -lt 0 -or $end -lt 0) { throw 'Interface-only probe missing' }
$probe = $source.Substring($start, $end - $start)
if (-not $probe.Contains('TryRaceMenu(messaging);')) { throw 'Probe does not request verbose exchange' }
foreach ($token in @('TryOBody', 'TrySLIF', 'InstallPush', 'RequestBake', 'ApplyVertexDiff', 'GetSingleton', 'RegisterEventSink')) {
    if ($probe.Contains($token)) { throw ('Early probe reaches game state: ' + $token) }
}
if (-not $main.Contains('case SKSE::MessagingInterface::kPostPostLoad:')) { throw 'OBody-equivalent phase missing' }
if (-not $main.Contains('SOS::MorphRefresh::ProbeRaceMenuInterface();')) { throw 'Phase does not invoke probe' }
foreach ($token in @('SKSE::GetPluginHandle()', 'sizeof(exchange), receiver', 'const bool dispatched =', 'exchange.interfaceMap != nullptr')) {
    if (-not $source.Contains($token)) { throw ('Probe telemetry missing: ' + $token) }
}
Write-Output 'INTERFACE_PROBE_GUARDS_OK: early phase queries only; sender/delivery/map results observable'
