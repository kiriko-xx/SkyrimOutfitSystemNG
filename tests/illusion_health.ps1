$ErrorActionPreference = 'Stop'
# Static guard for two answers the 1.5.85 audit demanded (1.5.86).
#
# 1) A call site whose fingerprint does not match is no longer hooked "because it is the only
#    one there". That sentence is what put 1.5.81 on a call site another plugin had rewritten;
#    the injected code read the actor out of a register that held something else and the cast
#    threw out of the hook. An unverified site now stays unhooked, and the hook that owns it
#    says in one line which feature that leaves off.
#
# 2) Hiding the real armour without ever attaching a piece of the outfit is a state that used
#    to be invisible: the log said the hooks were fine and the player was nude (1.5.82 hid the
#    skin as well and left a head only). Pieces hidden, passes with something to attach and
#    pieces that really landed are counted now; when the first keeps growing, the second keeps
#    happening and the third stays zero, the illusion is switched off, the real armour comes
#    back and a box says so.
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$failures = 0

function Read-RepoFile([string]$relative) {
    $path = Join-Path $root $relative
    if (-not (Test-Path $path)) { throw "missing $relative" }
    return (Get-Content -Raw -LiteralPath $path)
}

function Assert-Contains([string]$text, [string]$needle, [string]$label) {
    if ($text.Contains($needle)) { return }
    Write-Output "FAIL $label (missing: $needle)"
    $script:failures++
}

function Assert-Absent([string]$text, [string]$needle, [string]$label) {
    if (-not $text.Contains($needle)) { return }
    Write-Output "FAIL $label (unexpected: $needle)"
    $script:failures++
}

# 1) no hook is installed on an unverified call site any more
$support = Read-RepoFile 'src/cpp/hooking/HookSupport.cpp'
Assert-Absent $support 'hooking it anyway' 'a single unverified call to the callee is not patched on a guess (1.5.81)'
Assert-Absent $support 'hooking the first one at rva' 'a fingerprint that does not describe this build does not pick the first of several calls'
Assert-Contains $support 'so this call site could not be verified for this game build' 'an unverified site is named as such in the log'
Assert-Contains $support 'this hook stays disabled instead of being installed on a guess' 'an unverified site leaves the hook off'
$identity = [regex]::Matches($support, 'LogForeignJumps\(a_hook, entry, end\);').Count
if ($identity -lt 3) {
    Write-Output "FAIL every refusal should name the foreign jumps in the function (found $identity)"
    $failures++
}

# 2) the three hooks that still go through a call site say what their absence costs
$hooks = Read-RepoFile 'src/cpp/hooking/Hooks.cpp'
Assert-Contains $hooks 'ShimWornFlags: the worn mask shim stays off for this session' 'the worn mask shim reports the feature it leaves off'
Assert-Contains $hooks 'FixEquipConflictCheck: the equip conflict fix stays off for this session' 'the equip conflict fix reports the feature it leaves off'
Assert-Contains $hooks 'FixSkillLeveling: the skill level fix stays off for this session' 'the skill level fix reports the feature it leaves off'
Assert-Contains $hooks 'ShowIllusionUnavailableBox(true);' 'a session without the armour pass hook says so when the hooks are applied'

$hooksHeader = Read-RepoFile 'src/cpp/include/hooking/Hooks.hpp'
Assert-Contains $hooksHeader 'void ShowIllusionUnavailableBox(bool a_atLoad);' 'the box can be shown from the health check, not only at load'

# 3) hiding without attaching is counted and caught
$patches = Read-RepoFile 'src/cpp/hooking/Patches.cpp'
Assert-Contains $patches 'std::atomic<std::uint32_t> g_hiddenPieces{ 0 };' 'hidden pieces are counted'
Assert-Contains $patches 'std::atomic<std::uint32_t> g_attachPasses{ 0 };' 'passes that had something to attach are counted'
Assert-Contains $patches 'std::atomic<std::uint32_t> g_attachedPieces{ 0 };' 'pieces that really landed are counted'
Assert-Contains $patches 'kHiddenBeforeHealthCheck = 10' 'the health check waits for a few hidden pieces'
Assert-Contains $patches 'kPassesBeforeHealthCheck = 60' 'the health check waits for a few passes'
Assert-Contains $patches 'if (g_attachedPieces.load(std::memory_order_relaxed) != 0) return;' 'the health check stays quiet as soon as one piece landed'
Assert-Contains $patches 'SetIllusionDisplayEnabled(false);' 'a broken illusion is switched off rather than leaving the actor nude'
Assert-Contains $patches 'ShowIllusionUnavailableBox(false);' 'the player is told that the illusion was switched off'
if (([regex]::Matches($patches, 'g_hiddenPieces\.fetch_add\(1')).Count -ne 1) {
    Write-Output 'FAIL a hidden piece is counted exactly where the decision to hide is made'
    $failures++
}
if (([regex]::Matches($patches, 'g_attachPasses\.fetch_add\(1')).Count -ne 1) {
    Write-Output 'FAIL an attach attempt is counted once per pass'
    $failures++
}
if (([regex]::Matches($patches, 'g_attachedPieces\.fetch_add\(1')).Count -ne 1) {
    Write-Output 'FAIL a landed piece is counted once, where the slots are read back'
    $failures++
}
Assert-Contains $patches 'void CheckIllusionHealth()' 'the health check exists'
if (([regex]::Matches($patches, 'CheckIllusionHealth\(\);')).Count -ne 1) {
    Write-Output 'FAIL the health check runs once, at the end of an attach attempt'
    $failures++
}

$version = Read-RepoFile 'src/cpp/include/version.h'
if ($version -match 'SKYRIMOUTFITSYSTEMNG_VERSION_PATCH\s+(\d+)') {
    if ([int]$Matches[1] -lt 86) {
        Write-Output "FAIL the fail-closed call sites and the health check arrived in 1.5.86 (found 1.5.$($Matches[1]))"
        $failures++
    }
} else {
    Write-Output 'FAIL the patch version cannot be read'
    $failures++
}

if ($failures -ne 0) {
    Write-Output "ILLUSION_HEALTH_FAILED $failures"
    exit 1
}
Write-Output 'ILLUSION_HEALTH_OK'
