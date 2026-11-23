$ErrorActionPreference = 'Stop'
# Static guard for the AE 1.6.1170 attach path (1.5.60): the hook has to pick the
# call whose visitor is the one that fills the biped, the injected stub must not
# depend on any register the engine happens to hold, and a piece counts as attached
# only when the slots are read back - never by the bool ApplyArmorAddon returns.
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

$supportH = Read-RepoFile 'src/cpp/include/hooking/HookSupport.h'
Assert-Contains $supportH 'std::uintptr_t FindWornPassCall(' 'FindWornPassCall is declared'
Assert-Contains $supportH 'kAnchorLookback = 0x60' 'the anchor look-back window is declared'

$support = Read-RepoFile 'src/cpp/hooking/HookSupport.cpp'
Assert-Contains $support 'bool LoadsAddress(' 'the visitor anchor scan exists'
Assert-Contains $support '(bytes[2] & 0xC7) != 0x05' 'the anchor scan looks for rip-relative lea'
Assert-Contains $support 'std::uintptr_t FindWornPassCall(' 'FindWornPassCall is defined'
Assert-Contains $support 'a_anchorVTable' 'the site is chosen by the visitor vtable'
Assert-Contains $support 'stays disabled' 'an ambiguous site fails closed instead of guessing'

$hooks = Read-RepoFile 'src/cpp/hooking/Hooks.cpp'
# Only this hook is checked: other hooks legitimately pass a register through.
$customStart = $hooks.IndexOf('namespace CustomSkinPlayer {')
$customEnd = $hooks.IndexOf('namespace FixEquipConflictCheck')
if ($customStart -lt 0 -or $customEnd -le $customStart) { throw 'cannot locate the CustomSkinPlayer hook block' }
$customBlock = $hooks.Substring($customStart, $customEnd - $customStart)
Assert-Contains $hooks 'REL::RelocationID WornVisitorVTable(241890, 195851);' 'the worn visitor vtable is declared for both runtimes'
Assert-Contains $hooks 'FindWornPassCall("CustomSkinPlayer"' 'CustomSkinPlayer resolves its site through FindWornPassCall'
Assert-Contains $hooks 'SetWornVisitorVTable(WornVisitorVTable.address());' 'the resolved vtable is handed to the attach code'
Assert-Contains $customBlock 'mov(rcx, ptr[rsp + 8]);' 'the stub reads the inventory changes from the stack'
Assert-Contains $customBlock 'mov(rdx, ptr[rsp]);' 'the stub reads the visitor from the stack'
Assert-Contains $customBlock 'std::uintptr_t(CustomFromWornPass)' 'the stub calls the register-free entry point'
Assert-Absent $customBlock 'mov(rdx, rdi);' 'the stub no longer trusts rdi (AE holds a different object there)'
Assert-Absent $customBlock 'mov(rdx, r15);' 'the stub no longer trusts r15'

$patches = Read-RepoFile 'src/cpp/hooking/Patches.cpp'
Assert-Contains $patches 'void CustomFromWornPass(' 'CustomFromWornPass exists'
Assert-Contains $patches 'RecordSiteHookHit(actor->GetFormID());' 'a pass that reaches the actor is recorded'
Assert-Contains $patches 'reinterpret_cast<std::uintptr_t>(a_visitor) + 0x18' 'the biped is taken from the visitor layout'
Assert-Contains $patches 'std::uint32_t StagedSlots(' 'the applied slots are read back'
Assert-Contains $patches 'const auto staged = StagedSlots(anim, armor);' 'a piece is judged by what landed in the biped'
Assert-Absent $patches 'if (!RE::TESObjectARMOAugments::ApplyArmorAddon' 'ApplyArmorAddon no longer gates on its return value'
Assert-Contains $patches 'SiteHookHitRecently(id)' 'the deferred attach keeps running when the site hook never fires'

$version = Read-RepoFile 'src/cpp/include/version.h'
Assert-Contains $version 'SKYRIMOUTFITSYSTEMSE_VERSION_PATCH 60' 'the version is 1.5.60'

if ($failures -ne 0) {
    Write-Output "AE_WORN_PASS_FAILED $failures"
    exit 1
}
Write-Output 'AE_WORN_PASS_OK'
