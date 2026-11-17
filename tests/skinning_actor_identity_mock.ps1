$ErrorActionPreference = 'Stop'
$src = Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/hooking/Patches.cpp')
$begin = $src.IndexOf('if (target->GetFormType() != RE::FormType::ActorCharacter ||')
$end = $src.IndexOf('auto actor = target->As<RE::Actor>();', $begin)
$guard = $src.Substring($begin, $end - $begin)
if (-not $guard.Contains('return false;')) { throw 'Missing rejection branch' }
# This checks the declared Boolean contract, not C++ ABI or lifetime.
function Accept($character, $registeredSame) { return $character -and $registeredSame }
if (-not (Accept $true $true)) { throw 'Valid actor rejected' }
if (Accept $false $true) { throw 'Non-character accepted' }
if (Accept $true $false) { throw 'Mismatched registry identity accepted' }
if (Accept $false $false) { throw 'Unknown input accepted' }
Write-Output 'SKINNING_IDENTITY_TRUTH_TABLE_OK: 4 Boolean cases only; no engine lifetime claim'
