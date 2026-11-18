$ErrorActionPreference = 'Stop'
$root = Join-Path $PSScriptRoot '..'
$patches = Get-Content -Raw (Join-Path $root 'src/cpp/hooking/Patches.cpp')
$header = Get-Content -Raw (Join-Path $root 'src/cpp/include/hooking/Patches.hpp')
$morph = Get-Content -Raw (Join-Path $root 'src/cpp/MorphRefresh.cpp')

# A slot may only be torn out when this plugin put the clone there and nothing the
# engine owns sits behind it. 1.5.24 cleared whole display masks and left the actor
# without a body, so the three guards are asserted next to the call itself.
$detachCount = ($patches.Split('ClearBipedPart').Count - 1)
if ($detachCount -ne 1) { throw "Expected exactly one ClearBipedPart call site, found $detachCount" }
$call = $patches.IndexOf('ClearBipedPart')
$guardStart = [Math]::Max(0, $call - 1400)
$guards = $patches.Substring($guardStart, $call - $guardStart)
foreach ($required in @('stale.previousOwned', 'wornBySlot[bit] == nullptr', '(skinMask & flag) == 0', 'partClone')) {
  if (-not $guards.Contains($required)) { throw "Detach site is missing its guard: $required" }
}
if (-not $guards.Contains('ownedMask')) { throw 'Detach site is not scoped to slots this plugin injected' }
if (-not $patches.Contains('left to the engine')) { throw 'Engine-owned slots must still be left alone' }

# The reconcile entry has to be a cheap read: no writes, no engine calls besides the
# biped read, and no framework name (SAM, OStim, SexLab, OBody, RaceMenu) may appear.
$begin = $patches.IndexOf('std::uint32_t IllusionMissingSlots')
$end = $patches.IndexOf('void ForgetActor', $begin)
if ($begin -lt 0 -or $end -le $begin) { throw 'IllusionMissingSlots not found' }
$body = $patches.Substring($begin, $end - $begin)
foreach ($required in @('g_actorDisplayMask', 'GetBiped(false)', 'partClone', '!anim->root')) {
  if (-not $body.Contains($required)) { throw "Missing reconcile read: $required" }
}
foreach ($forbidden in @('AddTask', 'refreshArmorFor', 'ClearBipedPart', 'ApplyArmorAddon', 'SKSE::log')) {
  if ($body.Contains($forbidden)) { throw "Reconcile check must only read: $forbidden" }
}
if (-not $header.Contains('std::uint32_t IllusionMissingSlots(RE::Actor* a_actor);')) { throw 'Reconcile entry is not declared' }

# The rebuild itself reuses the ordinary equip refresh and stays framework agnostic.
$rb = $morph.IndexOf('void ReattachDroppedIllusion')
$rbEnd = $morph.IndexOf('void VerifyProviderPass', $rb)
if ($rb -lt 0 -or $rbEnd -le $rb) { throw 'ReattachDroppedIllusion not found' }
$rbBody = $morph.Substring($rb, $rbEnd - $rb)
foreach ($required in @('::OutfitSystem::refreshArmorFor(a_actor)', 'state.tries == 3', 'kReattachInterval')) {
  if (-not $rbBody.Contains($required)) { throw "Missing rebuild guard: $required" }
}
foreach ($framework in @('OStim', 'SexLab', 'SAM', 'OBody', 'RaceMenu', 'SLIF')) {
  if ($rbBody.Contains($framework)) { throw "Rebuild must not name a framework: $framework" }
}
if (-not $morph.Contains('Hooking::CustomSkinPlayer::IllusionMissingSlots(actor)')) { throw 'Poll must consult the reconcile check' }
if (-not $morph.Contains('Hooking::IllusionDisplayEnabled()')) { throw 'Reconcile must be gated on the illusion being active' }

Write-Output 'STALE_SLOT_GUARDS_OK: static only; engine behaviour unverified'
