$ErrorActionPreference = 'Stop'
# Static guard for every route by which the display set reaches a biped (1.5.88).
#
# The player log of 2026-11-27 was green from top to bottom on a repacked 1.6.1170 build: the
# armour pass hook was installed, the actor resolved, no exception was caught, no health check
# tripped - and the screen showed the real armour. Then the same build showed an outfit only
# once something was worn, because the display set was attached from inside the pass and only
# at its first piece that is not the skin. Three routes have to exist and stay in this order:
#
# 1) inside the pass (fast path, same frame)                                   - AttachFromHook
# 2) after the pass, on the weight model the pass handed over (1.5.88)        - AttachFromKnownModel
# 3) asking the engine to rebuild the biped, or filling it from outside        - refreshArmorFor / Custom
#
# Route 2 is what an actor who wears nothing has at all: its pass only carries the skin, the
# skin must not be hidden, so route 1 never runs for it.
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$failures = 0

function Read-RepoFile([string]$relative) {
    $path = Join-Path $root $relative
    if (-not (Test-Path $path)) { throw ('missing ' + $relative) }
    return (Get-Content -Raw -LiteralPath $path)
}

function Assert-Contains([string]$text, [string]$needle, [string]$label) {
    if ($text.Contains($needle)) { return }
    Write-Output ('FAIL ' + $label + ' (missing: ' + $needle + ')')
    $script:failures++
}

function Assert-Absent([string]$text, [string]$needle, [string]$label) {
    if (-not $text.Contains($needle)) { return }
    Write-Output ('FAIL ' + $label + ' (unexpected: ' + $needle + ')')
    $script:failures++
}

function Get-Block([string]$text, [string]$startMarker, [string]$endMarker, [string]$label) {
    $start = $text.IndexOf($startMarker)
    if ($start -lt 0) { throw ($label + ': start marker not found') }
    $end = $text.IndexOf($endMarker, $start)
    if ($end -lt 0) { throw ($label + ': end marker not found') }
    return $text.Substring($start, $end - $start)
}

$patches = Read-RepoFile 'src/cpp/hooking/Patches.cpp'

# 1) the actor of a pass is matched by biped pointer before any structure offset is trusted
Assert-Contains $patches 'std::unordered_map<const void*, RE::FormID> g_bipedOwner;' 'biped pointers are remembered per actor'
Assert-Contains $patches 'void RememberBipeds(RE::Actor* a_actor)' 'an actor can register both of its bipeds'
Assert-Contains $patches 'RE::Actor* ActorOfBiped(const void* a_biped)' 'a biped pointer can be turned back into its actor'
Assert-Contains $patches 'if (auto* known = ActorOfBiped(biped)) return known;' 'the pointer cache is asked before actorRef'
Assert-Contains $patches 'if (reinterpret_cast<const void*>(actor->GetBiped(false).get()) != a_biped &&' 'a cached actor has to still own that biped'
Assert-Contains $patches 'RememberBipeds(actor);// the fallback worked, so use the cache from now on' 'a successful actorRef lookup feeds the cache'

# 2) the weight model of the pass is remembered, so the deferred pass can attach to it
Assert-Contains $patches 'std::unordered_map<RE::FormID, std::pair<RE::ActorWeightModel*, std::chrono::steady_clock::time_point>> g_passModel;' 'the weight model of the last pass is kept per actor'
Assert-Contains $patches 'void NotePassModel(RE::Actor* a_actor, RE::ActorWeightModel* a_model)' 'a pass hands its model over'
Assert-Contains $patches 'if (!biped || biped != a_actor->GetBiped(false).get()) return;' 'only the third person model is kept - the first person one has hands only'
Assert-Contains $patches 'RE::ActorWeightModel* KnownPassModel(RE::Actor* a_actor)' 'the remembered model can be asked for'
Assert-Contains $patches 'kPassModelLifetime = std::chrono::seconds(30)' 'a remembered model expires instead of being trusted forever'
Assert-Contains $patches 'reinterpret_cast<RE::BipedAnim* const*>(model)[0] != a_actor->GetBiped(false).get()' 'the model is re-read on every use: a rebuilt biped leaves a stale pointer behind'
Assert-Contains $patches 'ForgetPassModel(id);' 'a stale model is dropped instead of being written through'

# 3) route 2 attaches to that model from the deferred pass
Assert-Contains $patches 'bool AttachFromKnownModel(RE::Actor* a_actor, RE::ActorWeightModel* a_model)' 'the deferred pass attaches to the handed-over model'
$knownBlock = Get-Block $patches 'bool AttachFromKnownModel(RE::Actor* a_actor' '// A pass for an actor with nothing worn' 'AttachFromKnownModel'
Assert-Contains $knownBlock '++g_attachDepth;' 'its own ApplyArmorAddon calls are not hidden'
Assert-Contains $knownBlock 'CustomOne(a_actor, a_model);' 'the display set goes on the model the engine draws'
Assert-Contains $knownBlock 'RecordSiteHookHit(a_actor->GetFormID());' 'a filled biped is recorded, so the next frames do not repeat it'
Assert-Contains $knownBlock 'the display set was attached from the deferred pass with the weight model the armour pass handed over' 'the route speaks once in the log'

# 4) the deferred pass tries them in order, and keeps the 1.5.87 fallbacks behind it
$drainBlock = Get-Block $patches 'void DrainAttachPending()' 'void SetSiteHookInstalled' 'DrainAttachPending'
Assert-Contains $drainBlock 'if (SiteHookInstalled() && SiteHookHitRecently(id)) continue;' 'a biped filled a moment ago is left alone'
Assert-Contains $drainBlock 'if (auto* model = KnownPassModel(actor)) {' 'the handed-over model is asked for first'
Assert-Contains $drainBlock 'if (AttachFromKnownModel(actor, model)) continue;' 'and used when it is still valid'
Assert-Contains $drainBlock 'if (HookProven(id)) {' 'without a model the refresh is still there'
Assert-Contains $drainBlock '::OutfitSystem::refreshArmorFor(actor);' 'the engine rebuilds the biped when the hook does run for the actor'
Assert-Contains $drainBlock 'Custom(actor, nullptr);' 'the external fill stays as the last fallback'
Assert-Contains $drainBlock 'LogThrown("DrainAttachPending");' 'an exception in the deferred pass cannot take the game down'
$modelIndex = $drainBlock.IndexOf('KnownPassModel(actor)')
$refreshIndex = $drainBlock.IndexOf('refreshArmorFor(actor)')
$outsideIndex = $drainBlock.IndexOf('Custom(actor, nullptr)')
if ($modelIndex -lt 0 -or $refreshIndex -lt 0 -or $outsideIndex -lt 0 -or -not ($modelIndex -lt $refreshIndex -and $refreshIndex -lt $outsideIndex)) {
    Write-Output 'FAIL the deferred pass has to try the handed-over model, then a rebuild, then the external fill'
    $failures++
}
Assert-Absent $drainBlock 'if (SiteHookInstalled() && HookProven(id)) {' 'the refresh no longer comes before the model the pass handed over'
Assert-Absent $patches 'if (SiteHookInstalled() && HookProven(id) && SiteHookHitRecently(id)) return;' 'a filled biped is no longer gated on the hook having been proven first'

# 5) a pass that only carries the skin owes the deferred attach
$decideBlock = Get-Block $patches 'bool ShouldHideForModel(RE::TESObjectARMO* a_armor' '}// namespace DontVanillaSkinPlayer' 'the armour pass decision'
Assert-Contains $decideBlock 'NotePassModel(actor, a_model);' 'the model is remembered on every pass, not only on a pass that hides something'
Assert-Contains $decideBlock 'if (a_armor == actor->GetSkin()) {' 'the skin is still never hidden'
Assert-Contains $decideBlock 'NoteSkinOnlyPass(actor);' 'a skin-only pass says so once'
Assert-Contains $decideBlock 'MarkAttachPending(actor);' 'and asks for the deferred attach'
$modelCall = $decideBlock.IndexOf('NotePassModel(actor, a_model);')
$skinCheck = $decideBlock.IndexOf('if (a_armor == actor->GetSkin()) {')
$attachCall = $decideBlock.IndexOf('AttachFromHook(actor, a_model);')
if ($modelCall -lt 0 -or $skinCheck -lt 0 -or $attachCall -lt 0 -or -not ($modelCall -lt $skinCheck -and $skinCheck -lt $attachCall)) {
    Write-Output 'FAIL the model is remembered before the skin check, and the in-pass attach stays after it'
    $failures++
}

# 6) the silent answers and the silent pass still speak, and a nude pass is not a fault
Assert-Contains $patches 'ShouldOverrideSkinning: the illusion is switched off for this session, so {:08X} is left alone.' 'a session with the illusion off says so'
Assert-Contains $patches 'ShouldOverrideSkinning: the outfit service is disabled, so {:08X} is left alone.' 'a disabled service says so'
Assert-Contains $patches 'ShouldOverrideSkinning: no outfit is assigned to {:08X}, so nothing is hidden and nothing is attached for it.' 'an actor without an outfit says so'
$noteBlock = Get-Block $patches 'void NoteArmourPass(RE::Actor* a_actor)' 'void CheckIllusionHealth()' 'NoteArmourPass'
Assert-Contains $noteBlock 'the armour pass has run {} time(s) for the player and not one piece of real armour was hidden' 'a pass that hides nothing is reported once'
Assert-Contains $noteBlock 'if (g_attachedPieces.load(std::memory_order_relaxed) != 0) return;' 'a pass that attached the display set is not reported as broken'
Assert-Contains $patches 'from inside the pass, so the illusion no longer depends on the worn visitor hook' 'a successful in-pass attach is reported once'
if (([regex]::Matches($patches, 'LogAttachRoute\(')).Count -lt 4) {
    Write-Output 'FAIL every deferred route has to explain itself once per actor'
    $failures++
}

$version = Read-RepoFile 'src/cpp/include/version.h'
if ($version -match 'SKYRIMOUTFITSYSTEMNG_VERSION_PATCH\s+(\d+)') {
    if ([int]$Matches[1] -lt 88) {
        Write-Output ('FAIL the handed-over model route arrived in 1.5.88 (found 1.5.' + $Matches[1] + ')')
        $failures++
    }
} else {
    Write-Output 'FAIL the patch version cannot be read'
    $failures++
}

if ($failures -ne 0) {
    Write-Output ('ATTACH_PATHS_FAILED ' + $failures)
    exit 1
}
Write-Output 'ATTACH_PATHS_OK'
