$ErrorActionPreference='Stop'
$source=Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/MorphRefresh.cpp')
$start=$source.IndexOf('for (const auto id : applyNow)')
$end=$source.IndexOf('bool TryRaceMenu(', $start)
if($start -lt 0 -or $end -lt 0){throw 'Deferred consumer missing'}
$consumer=$source.Substring($start,$end-$start)
if(-not $consumer.Contains('RequestBake(id, BakeReason::kRebuilt);')){throw 'Clone-only request missing'}
foreach($token in @('ApplyMorphs(', 'ApplyBodyMorphs(', 'RefreshActor(', 'CycleIllusion(')){if($consumer.Contains($token)){throw ('Broad mutation: '+$token)}}
if(-not $consumer.Contains('g_applyScheduled.erase(id);')){throw 'Scheduled token not released'}
Write-Output 'REBUILT_SCOPE_OK: clone-only delayed request; static guard, not engine test'
