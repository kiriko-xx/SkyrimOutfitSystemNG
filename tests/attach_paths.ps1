$ErrorActionPreference = 'Stop'
# Static guard for the one route the display set is allowed to take (1.5.89).
#
# The player log of 2026-11-27 settled the question the hard way. On a 1.6.1170 build every
# slot line the plugin wrote read 'part 1 clone 0' - 368 of them, not one 'clone 1' - so the
# slots were claimed and no mesh ever appeared: an empty preset drew a head only, and a hair
# slot was taken with no hair in it, which turned the character bald.
#
# The reason is who finishes the parts. The routine that CALLS InventoryChanges::
# ExecuteVisitorOnWorn is the one that turns what the biped holds into geometry once the
# visitor returns. A biped written before the visitor returns is finished by that routine; a
# biped written after it (1.5.82, from inside ApplyArmorAddon) or outside the pass altogether
# (1.5.87 and 1.5.88, from a deferred queue) is never finished, whatever its slots say.
#
# So the display set is written from exactly one place - the worn visitor hook - and the
# deferred queue only asks the engine for a rebuild, which runs the pass again.
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
$hooks = Read-RepoFile 'src/cpp/hooking/Hooks.cpp'
$header = Read-RepoFile 'src/cpp/include/hooking/Patches.hpp'

# 1) the worn visitor hook is the only writer of the display set
Assert-Contains $patches 'void CustomFromWornPass(RE::InventoryChanges* a_inventory, RE::InventoryChanges::IItemChangeVisitor* a_visitor)' 'the worn pass entry point exists'
Assert-Contains $header 'void CustomFromWornPass(RE::InventoryChanges* inventory, RE::InventoryChanges::IItemChangeVisitor* visitor);' 'and is reachable from the hook'
$passBlock = Get-Block $patches 'void CustomFromWornPass(RE::InventoryChanges* a_inventory' '// One line per actor and route' 'CustomFromWornPass'
Assert-Contains $passBlock 'static thread_local bool inPass = false;' 'a pass that calls itself cannot recurse'
Assert-Contains $passBlock 'auto* anim = PassBiped(a_visitor);' 'the biped comes out of the visitor the game handed over'
Assert-Contains $passBlock 'if (!anim) {' 'a visitor of another pass is refused'
Assert-Contains $passBlock 'LogForeignVisitor(a_visitor);' 'and named in the log'
Assert-Contains $passBlock 'if (!ShouldOverrideSkinning(owner)) return;' 'only actors with an assigned outfit are touched'
Assert-Contains $passBlock 'RecordSiteHookHit(actor->GetFormID());' 'a filled biped is recorded'
Assert-Contains $passBlock 'MarkHookProven(actor->GetFormID());' 'and the hook is known to reach this actor'
Assert-Contains $passBlock '++g_attachDepth;' 'its own ApplyArmorAddon calls are not hidden'
Assert-Contains $passBlock 'CustomOne(actor, reinterpret_cast<RE::ActorWeightModel*>(&holder));' 'the display set is written here and nowhere else'
Assert-Contains $passBlock 'LogThrown("CustomFromWornPass");' 'an exception in the pass cannot take the game down'
Assert-Contains $passBlock 'the worn pass hook has been reached, so the display set is attached from inside the pass that fills the biped.' 'the route speaks once in the log'

# 2) the biped is addressed through the visitor, never through a register or a stale model
$bipedBlock = Get-Block $patches 'RE::BipedAnim* PassBiped(RE::InventoryChanges::IItemChangeVisitor* a_visitor)' '// Same blind spot one level deeper' 'PassBiped'
Assert-Contains $bipedBlock 'g_wornVisitorVTable.load(std::memory_order_relaxed);' 'the visitor vtable is the gate'
Assert-Contains $bipedBlock 'if (*reinterpret_cast<const std::uintptr_t*>(a_visitor) != vtable) return nullptr;' 'a visitor of a checking pass is not read'
Assert-Contains $bipedBlock 'return *reinterpret_cast<RE::BipedAnim**>(reinterpret_cast<std::uintptr_t>(a_visitor) + 0x18);' 'the biped is the visitor member at 0x18'
Assert-Contains $patches 'void SetWornVisitorVTable(std::uintptr_t a_vtable)' 'the vtable of the filling pass can be handed over'
Assert-Contains $patches 'the hooked worn pass was handed a visitor with vtable 0x{:X}, but this build records 0x{:X}' 'an unexpected visitor is named once'

# 3) the hook sits on the worn visitor, and the vanilla pass runs first
Assert-Contains $hooks 'REL::RelocationID InventoryChanges_ExecuteVisitorOnWorn(15856, 16096);' 'the worn visitor is addressed through the address library'
Assert-Contains $hooks 'REL::RelocationID WornVisitorVTable(241890, 195851);' 'and so is the visitor vtable'
Assert-Contains $hooks 'SetWornVisitorVTable(WornVisitorVTable.address());' 'the vtable is handed to the C++ side'
Assert-Contains $hooks 'FollowEntryJumps("CustomSkinPlayer", gameEntry)' 'another plugin already on the function is chained onto'
# The armour-pass stub uses the same shape, so the worn visitor stub is cut out by its own
# tail: everything from the RelocationID declaration up to the code generator instance.
$stubStart = $hooks.IndexOf('REL::RelocationID InventoryChanges_ExecuteVisitorOnWorn(15856, 16096);')
$stubEnd = $hooks.IndexOf('std::uintptr_t(CustomFromWornPass)', $stubStart)
if ($stubStart -lt 0 -or $stubEnd -lt 0 -or $stubEnd -le $stubStart) {
    Write-Output 'FAIL the worn visitor hook block cannot be found'
    $failures++
    $stubBlock = ''
} else {
    $stubBlock = $hooks.Substring($stubStart, $stubEnd - $stubStart)
}
$trampolineCall = $stubBlock.IndexOf('call(ptr[rip + f_trampoline]);')
$customCall = $stubBlock.IndexOf('call(ptr[rip + f_CustomFromWornPass]);')
if ($trampolineCall -lt 0 -or $customCall -lt 0 -or -not ($trampolineCall -lt $customCall)) {
    Write-Output 'FAIL the vanilla pass has to run before the display set is written'
    $failures++
}
Assert-Contains $stubBlock 'sub(rsp, 0x28);' 'the callee gets shadow space and a 16-byte aligned stack'
Assert-Contains $hooks 'g_customSkinPlayerInstalled = Install("custom skin player", entry, gen);' 'the result of the install is kept'
Assert-Contains $hooks 'SetSiteHookInstalled(g_customSkinPlayerInstalled);' 'and handed to the deferred queue'
Assert-Contains $hooks 'the worn visitor is hooked at {} (declared at {}), so the display set is attached inside the pass that fills the biped.' 'a working hook says where it sits'
Assert-Contains $hooks 'the worn visitor of this game build could not be hooked' 'a failed hook says what is lost'

# 4) the armour pass hides and does not attach
Assert-Contains $hooks 'g_dontVanillaSkinPlayerInstalled = Install("armour pass", entry, gen);' 'the armour pass hook still hides'
$decideBlock = Get-Block $patches 'bool ShouldHideForModel(RE::TESObjectARMO* a_armor' '}// namespace DontVanillaSkinPlayer' 'the armour pass decision'
Assert-Contains $decideBlock 'if (a_armor == actor->GetSkin()) {' 'the skin is still never hidden'
Assert-Contains $decideBlock 'MarkAttachPending(actor);' 'a skin-only pass asks for the pass to run again'
Assert-Contains $decideBlock 'return ShouldOverride(a_armor, actor);' 'every other piece still goes through the slot policies'
Assert-Contains $decideBlock 'LogThrown("ShouldHideForModel");' 'an exception here cannot take the game down'
Assert-Absent $decideBlock 'AttachFromHook' 'the armour pass no longer attaches anything'
Assert-Absent $decideBlock 'AttachFromKnownModel' 'and neither does a handed-over model route'
Assert-Absent $decideBlock 'NotePassModel' 'the 1.5.88 model cache is gone'

# 5) the deferred queue never writes a biped
$drainBlock = Get-Block $patches 'void DrainAttachPending()' 'void SetSiteHookInstalled' 'DrainAttachPending'
Assert-Contains $drainBlock 'if (SiteHookInstalled() && SiteHookHitRecently(id)) continue;' 'a biped filled a moment ago is left alone'
Assert-Contains $drainBlock '::OutfitSystem::refreshArmorFor(actor);' 'the engine is asked to rebuild the biped'
Assert-Contains $drainBlock 'has not been through the hooked worn pass in the last moment; asking the engine to rebuild its biped so that the pass can fill it.' 'the rebuild route explains itself'
Assert-Contains $drainBlock 'the display set is never written from outside the pass, so it is left alone' 'an actor the pass never reached is left alone'
Assert-Contains $drainBlock 'LogThrown("DrainAttachPending");' 'an exception in the queue cannot take the game down'
Assert-Absent $drainBlock 'Custom(actor, nullptr)' 'the external fill is gone: it produced slots without geometry'
Assert-Absent $drainBlock 'AttachFromKnownModel' 'the handed-over model route is gone'
Assert-Absent $patches 'AttachFromHook' 'no route attaches from inside the armour pass'
Assert-Absent $patches 'AttachFromKnownModel' 'no route attaches to a remembered model'
Assert-Absent $patches 'g_passModel' 'no pass model is remembered'
Assert-Absent $patches 'NoteSkinOnlyPass' 'a skin-only pass is not tracked separately'

# 6) the silent answers still speak
Assert-Contains $patches 'ShouldOverrideSkinning: the illusion is switched off for this session, so {:08X} is left alone.' 'a session with the illusion off says so'
Assert-Contains $patches 'ShouldOverrideSkinning: no outfit is assigned to {:08X}, so nothing is hidden and nothing is attached for it.' 'an actor without an outfit says so'
Assert-Contains $patches 'the armour pass has run {} time(s) for the player and not one piece of real armour was hidden' 'a pass that hides nothing is reported once'
if (([regex]::Matches($patches, 'LogAttachRoute\(')).Count -lt 4) {
    Write-Output 'FAIL every deferred route has to explain itself once per actor'
    $failures++
}

# 7) both hooks are required, and each names its own loss
Assert-Contains $hooks 'Detail::CustomSkinPlayer::Apply();' 'the worn visitor hook is installed'
Assert-Contains $hooks 'Detail::DontVanillaSkinPlayer::Apply();' 'the hiding hook is installed'
$version = Read-RepoFile 'src/cpp/include/version.h'
if ($version -match 'SKYRIMOUTFITSYSTEMNG_VERSION_PATCH\s+(\d+)') {
    if ([int]$Matches[1] -lt 89) {
        Write-Output ('FAIL writing the biped only from inside the pass arrived in 1.5.89 (found 1.5.' + $Matches[1] + ')')
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
