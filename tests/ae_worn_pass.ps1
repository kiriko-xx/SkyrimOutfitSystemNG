$ErrorActionPreference = 'Stop'
# Static guard for the AE 1.6.1170 attach path (1.5.61): the skinning pass is reached by
# hooking the worn visitor at its own entry (no call site inside the game routine, which
# other plugins patch as well), the trampoline copies a prologue that is safe to relocate,
# the stub depends on no register the engine happens to hold, only the filling pass (by
# visitor vtable) may write the biped, and the deferred pass never fills a biped itself
# while the hook is installed - that is what produced the see-through body on 1.6.1170.
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

function Get-Block([string]$text, [string]$startMarker, [string]$endMarker, [string]$label) {
    $start = $text.IndexOf($startMarker)
    $end = $text.IndexOf($endMarker)
    if ($start -lt 0 -or $end -le $start) { throw "cannot locate $label" }
    return $text.Substring($start, $end - $start)
}

$supportH = Read-RepoFile 'src/cpp/include/hooking/HookSupport.h'
Assert-Contains $supportH 'std::size_t CopyableEntryLength(' 'the prologue copy helper is declared'

$support = Read-RepoFile 'src/cpp/hooking/HookSupport.cpp'
Assert-Contains $support 'std::size_t CopyableEntryLength(' 'the prologue copy helper is defined'
Assert-Contains $support 'at[2] == 0x5C' 'only stack stores of callee-saved registers are copied'
Assert-Contains $support 'at[1] >= 0x50' 'pushes of volatile registers are allowed'
Assert-Contains $support 'length > left' 'an instruction that does not end on the copied boundary is refused'

$hooks = Read-RepoFile 'src/cpp/hooking/Hooks.cpp'
$customBlock = Get-Block $hooks 'namespace CustomSkinPlayer {' 'namespace FixEquipConflictCheck' 'the CustomSkinPlayer hook block'
Assert-Contains $hooks 'REL::RelocationID InventoryChanges_ExecuteVisitorOnWorn(15856, 16096);' 'the worn visitor is addressed through the address library'
Assert-Contains $hooks 'REL::RelocationID WornVisitorVTable(241890, 195851);' 'the worn visitor vtable is declared for both runtimes'
Assert-Contains $customBlock 'CopyableEntryLength(entry, kStolen)' 'the entry prologue is checked before it is copied'
Assert-Contains $customBlock 'db(reinterpret_cast<const std::uint8_t*>(a_entry), a_copied);' 'the trampoline carries a copy of the game prologue'
Assert-Contains $customBlock 'call(ptr[rip + f_trampoline]);' 'the stub calls the copied prologue instead of the game function'
Assert-Contains $customBlock 'push(rcx);' 'the stub keeps the arguments across the vanilla pass'
Assert-Contains $customBlock 'push(rdx);' 'the stub keeps the visitor across the vanilla pass'
Assert-Contains $customBlock 'mov(rcx, ptr[rsp + 8]);' 'the stub reads the inventory changes from the stack'
Assert-Contains $customBlock 'mov(rdx, ptr[rsp]);' 'the stub reads the visitor from the stack'
Assert-Contains $customBlock 'ret();' 'the entry stub returns to the caller of the game function'
Assert-Contains $customBlock 'Install("custom skin player", entry, gen)' 'the hook is written to the entry of the worn visitor'
Assert-Contains $customBlock 'SetWornVisitorVTable(WornVisitorVTable.address());' 'the resolved vtable is handed to the attach code'
Assert-Absent $customBlock 'FindWornPassCall(' 'the call site inside the game routine is no longer scanned'
Assert-Absent $customBlock 'mov(rdx, rdi);' 'the stub no longer trusts rdi (AE holds a different object there)'
Assert-Absent $customBlock 'mov(rdx, r15);' 'the stub no longer trusts r15'
Assert-Contains $customBlock 'jmp(ptr[rip]);' 'the trampoline jumps back into the game function'

$patches = Read-RepoFile 'src/cpp/hooking/Patches.cpp'
$attachBlock = Get-Block $patches 'void CustomFromWornPass(' '// Called from the main thread, outside any skinning pass.' 'CustomFromWornPass'
Assert-Contains $attachBlock 'PassBiped(a_visitor)' 'the pass is identified by the visitor it was handed'
Assert-Contains $attachBlock 'if (!anim) return;' 'another pass is left alone instead of filling a foreign biped'
Assert-Absent $attachBlock 'actor->GetBiped(false).get()' 'no fallback to the third person biped from inside the hook'
Assert-Contains $attachBlock 'static thread_local bool inPass = false;' 're-entry from the pass itself is blocked'
Assert-Contains $attachBlock 'RecordSiteHookHit(actor->GetFormID());' 'a pass that reaches the actor is recorded'
Assert-Contains $patches 'reinterpret_cast<std::uintptr_t>(a_visitor) + 0x18' 'the biped is taken from the visitor layout'
Assert-Contains $patches 'std::uint32_t StagedSlots(' 'the applied slots are read back'
Assert-Contains $patches 'const auto staged = StagedSlots(anim, armor);' 'a piece is judged by what landed in the biped'
Assert-Absent $patches 'if (!RE::TESObjectARMOAugments::ApplyArmorAddon' 'ApplyArmorAddon no longer gates on its return value'
Assert-Contains $patches 'SiteHookHitRecently(id)' 'the deferred attach keeps running when the hook never fires'
$drainBlock = Get-Block $patches 'void DrainAttachPending()' 'void SetSiteHookInstalled' 'DrainAttachPending'
Assert-Contains $drainBlock '::OutfitSystem::refreshArmorFor(actor);' 'the deferred pass rebuilds the equipment while the hook is installed'
Assert-Contains $drainBlock 'Custom(actor, nullptr);' 'the external fill stays as the fallback without an in-pass hook'

$version = Read-RepoFile 'src/cpp/include/version.h'
if ($version -match 'SKYRIMOUTFITSYSTEMSE_VERSION_PATCH\s+(\d+)') {
    $patch = [int]$Matches[1]
    if ($patch -lt 61) {
        Write-Output "FAIL the worn visitor entry hook arrived in 1.5.61 (found 1.5.$patch)"
        $failures++
    }
} else {
    Write-Output 'FAIL the patch version cannot be read'
    $failures++
}

if ($failures -ne 0) {
    Write-Output "AE_WORN_PASS_FAILED $failures"
    exit 1
}
Write-Output 'AE_WORN_PASS_OK'
