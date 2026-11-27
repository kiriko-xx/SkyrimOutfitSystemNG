$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$hooks   = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/hooking/Hooks.cpp')
$support = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/hooking/HookSupport.cpp')
$header  = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/include/hooking/HookSupport.h')
$pattern = Get-Content -Raw -LiteralPath (Join-Path $root 'src/cpp/include/hooking/SitePattern.h')
$fails = 0
function Missing([string]$text, [string]$needle, [string]$what) {
    if (-not $text.Contains($needle)) { Write-Host "FAIL $what"; $script:fails++ }
}
function Present([string]$text, [string]$needle, [string]$what) {
    if ($text.Contains($needle)) { Write-Host "FAIL $what"; $script:fails++ }
}
function Block([string]$text, [string]$start, [string]$end) {
    $i = $text.IndexOf($start)
    if ($i -lt 0) { throw "block start not found: $start" }
    $j = $text.IndexOf($end, $i)
    if ($j -lt 0) { throw "block end not found: $end" }
    return $text.Substring($i, $j - $i)
}

# every call site hook goes through the fingerprint check
$verified = ([regex]::Matches($hooks, 'FindVerifiedCall\(')).Count
if ($verified -ne 3) { Write-Host "FAIL the three remaining call site hooks should all use FindVerifiedCall, found $verified"; $fails++ }
Missing $hooks 'LocateEntry("DontVanillaSkinPlayer"' 'the armour pass entry is checked against the fingerprint it must hold before it is written'
Missing $hooks 'REL::RelocationID TESObjectARMO_ApplyArmorAddon(17392, 17792);' 'the armour pass is hooked at its own entry'
Missing $hooks 'kArmourPassFingerprint' 'the entry of the armour pass is checked against the fingerprint it must hold'
Present $hooks 'FindVerifiedCall("DontVanillaSkinPlayer"' 'the armour pass is no longer reached through a call site fingerprint'
Present $hooks 'FindCallInFunction(' 'Hooks.cpp no longer scans a host body without a fingerprint'
Present $hooks 'FindCallInFunctionOrOffset(' 'Hooks.cpp no longer falls back to a fixed offset'
Present $hooks '0x7B' 'the skill loop skip point is derived, not hard coded'

# the fingerprints themselves (measured on 1.5.97 and 1.6.1170)
Present $hooks '44 8B 49 38 41 83 E1 01 48 8B 91 58 01 00 00 48 8B CD' 'the vanilla armour pass fingerprint is gone, the hook sits on the function entry'
Present $hooks '40 B? 01 40 0F B6 C?' 'the dil/sil tolerant fingerprint is gone as well'
Missing $hooks '48 8B C8 4C 89 ?? 24 70' 'the worn mask fingerprint tolerates the lock register'
Missing $hooks '4C 8B F0 8B D3 48 8B CD' 'the equip conflict fingerprint is declared'
Missing $hooks '4C 8B E8 48 8B 0E' 'the AE skill loop fingerprint is declared'
Missing $hooks '48 8D 54 24 20 48 89 44 24 28' 'the SE skill call fingerprint is declared'

# the skill hook derives its skip point
$skill = Block $hooks 'namespace FixSkillLeveling' '#endif'
Missing $skill 'DeriveLoopSkip(' 'FixSkillLeveling derives the skip point from the loop'
Missing $skill 'SkillMutationFunction_Hook + 0x5' 'FixSkillLeveling resumes right after the call'
Present $skill '0x7B' 'FixSkillLeveling does not hard code the skip point'

# the support layer, both halves
Missing $header 'struct SiteShape' 'SiteShape is declared'
Missing $header 'FindVerifiedCall(' 'FindVerifiedCall is declared'
Missing $header 'DeriveLoopSkip(' 'DeriveLoopSkip is declared'
Missing $support 'is the site of this hook' 'a matching site is logged'
Missing $support 'does not match' 'an unmatched single candidate is logged'
$guess = ([regex]::Matches($support, 'hooking it anyway')).Count
if ($guess -ne 0) { Write-Host 'FAIL an unverified single call site is no longer hooked on a guess (1.5.81 crash)'; $fails++ }
Missing $support 'so this call site could not be verified for this game build' 'an unverified site is refused and named'
Missing $support 'target < address' 'the loop scan accepts a back edge that lands after the call'
$disabled = ([regex]::Matches($support, 'the hook stays disabled')).Count
if ($disabled -lt 1) { Write-Host 'FAIL an ambiguous site must disable the hook instead of guessing'; $fails++ }

# the pure header that does the matching
Missing $pattern 'namespace Hooking::SitePattern' 'SitePattern lives in its own header'
Missing $pattern 'HexDigit' 'SitePattern parses hex digits'
Missing $pattern 'MatchesAll' 'SitePattern matches a whole shape'
if ($pattern -match '#include <RE/') { Write-Host 'FAIL SitePattern must stay free of game headers so the unit test can build it'; $fails++ }

# the unit test that pins the fingerprints
if (-not (Test-Path (Join-Path $root 'tests/site_pattern.cpp'))) { Write-Host 'FAIL tests/site_pattern.cpp is missing'; $fails++ }
if (-not (Test-Path (Join-Path $root 'tests/site_pattern.ps1'))) { Write-Host 'FAIL tests/site_pattern.ps1 is missing'; $fails++ }

if ($fails -gt 0) { Write-Host "SITE_GUARD_FAILED: $fails check(s)"; exit 1 }
Write-Host 'SITE_GUARD_OK: 3 hook(s) verified by fingerprint, 2 hooked at an entry found by the 33 byte fingerprint of the function'