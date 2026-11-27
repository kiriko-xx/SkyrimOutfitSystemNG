$ErrorActionPreference = 'Stop'
# Static guard for the three ways 1.5.87 keeps an illusion visible (or explains why it is not).
#
# The player log of 2026-11-27 was green from top to bottom on a repacked 1.6.1170 build: the
# armour pass hook was installed, the actor resolved, no exception was caught, no health check
# tripped - and the screen showed the real armour. Three things were missing:
#
# 1) The actor of a pass was reached only through BipedAnim::actorRef, one structure offset.
#    Every actor this plugin sees now registers its two biped pointers, and a weight model is
#    matched by pointer first; actorRef is the fallback for an actor never seen before.
# 2) The deferred pass asked the engine to rebuild the biped as soon as the hook existed, but a
#    hook that never fills this actor on this build makes that refresh invisible work. It now
#    needs the hook to have been proven for that actor, otherwise the external fill is used -
#    the path that still made the illusion appear on that machine in 1.5.81.
# 3) "Nothing happened" was unloggable: no outfit assigned, service disabled and illusion off
#    all returned false in silence. Each says one debug line now, and a pass that runs for the
#    player without hiding a single piece says so once at warn level.
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

$patches = Read-RepoFile 'src/cpp/hooking/Patches.cpp'

# 1) the actor of a pass is matched by biped pointer before any structure offset is trusted
Assert-Contains $patches 'std::unordered_map<const void*, RE::FormID> g_bipedOwner;' 'biped pointers are remembered per actor'
Assert-Contains $patches 'void RememberBipeds(RE::Actor* a_actor)' 'an actor can register both of its bipeds'
Assert-Contains $patches 'RE::Actor* ActorOfBiped(const void* a_biped)' 'a biped pointer can be turned back into its actor'
Assert-Contains $patches 'if (auto* known = ActorOfBiped(biped)) return known;' 'the pointer cache is asked before actorRef'
Assert-Contains $patches 'if (reinterpret_cast<const void*>(actor->GetBiped(false).get()) != a_biped &&' 'a cached actor has to still own that biped'
Assert-Contains $patches 'RememberBipeds(actor);// the fallback worked, so use the cache from now on' 'a successful actorRef lookup feeds the cache'
if (([regex]::Matches($patches, 'RememberBipeds\(actor\);')).Count -lt 3) {
    Write-Output 'FAIL every actor this plugin touches should end up in the biped cache'
    $failures++
}

# 2) the deferred pass only refreshes what the hook has proven, otherwise it fills from outside
Assert-Contains $patches 'std::unordered_set<RE::FormID> g_provenActors;' 'proven actors are remembered'
Assert-Contains $patches 'bool HookProven(RE::FormID a_actor)' 'proven-ness can be asked'
Assert-Contains $patches 'if (SiteHookInstalled() && HookProven(id)) {' 'the refresh waits for a hook proven on this actor'
Assert-Contains $patches 'if (SiteHookInstalled() && HookProven(id) && SiteHookHitRecently(id)) return;' 'a hook hit alone no longer suppresses the pending attach'
Assert-Absent $patches 'if (SiteHookInstalled()) {' 'the deferred pass no longer refreshes just because a hook exists'
Assert-Absent $patches 'if (SiteHookInstalled() && SiteHookHitRecently(id)) return;' 'a hook hit without proof no longer suppresses the pending attach'
Assert-Contains $patches 'MarkHookProven(a_actor->GetFormID());' 'the hook marks an actor proven only when it actually runs for it'

# 3) the silent answers and the silent pass now speak
Assert-Contains $patches 'ShouldOverrideSkinning: the illusion is switched off for this session, so {:08X} is left alone.' 'a session with the illusion off says so'
Assert-Contains $patches 'ShouldOverrideSkinning: the outfit service is disabled, so {:08X} is left alone.' 'a disabled service says so'
Assert-Contains $patches 'ShouldOverrideSkinning: no outfit is assigned to {:08X}, so nothing is hidden and nothing is attached for it.' 'an actor without an outfit says so'
Assert-Contains $patches 'inline constexpr RE::FormID kPlayerFormID = 0x14;' 'the player is the one actor worth a warning'
Assert-Contains $patches 'NoteArmourPass(actor);' 'every armour pass for the player is counted'
Assert-Contains $patches 'the armour pass has run {} time(s) for the player and not one piece of real armour was hidden' 'a pass that hides nothing is reported once'
Assert-Contains $patches 'from inside the pass, so the illusion no longer depends on the worn visitor hook' 'a successful in-pass attach is reported once'

$version = Read-RepoFile 'src/cpp/include/version.h'
if ($version -match 'SKYRIMOUTFITSYSTEMNG_VERSION_PATCH\s+(\d+)') {
    if ([int]$Matches[1] -lt 87) {
        Write-Output "FAIL the biped cache and the proven-hook routing arrived in 1.5.87 (found 1.5.$($Matches[1]))"
        $failures++
    }
} else {
    Write-Output 'FAIL the patch version cannot be read'
    $failures++
}

if ($failures -ne 0) {
    Write-Output "ATTACH_PATHS_FAILED $failures"
    exit 1
}
Write-Output 'ATTACH_PATHS_OK'
