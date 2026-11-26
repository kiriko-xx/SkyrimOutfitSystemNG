$ErrorActionPreference = 'Stop'
# Static guard for the AE 1.6.1170 attach path (1.5.61): the skinning pass is reached by
# hooking the worn visitor at its own entry (no call site inside the game routine, which
# other plugins patch as well), the trampoline copies a prologue that is safe to relocate,
# the stub depends on no register the engine happens to hold, only the filling pass (by
# visitor vtable) may write the biped, the deferred pass never fills a biped itself while
# the hook is installed - that is what produced the see-through body on 1.6.1170 - and a
# site that another plugin moved out of the executable is reached through a stub next to
# the site, because the trampoline sits next to the game and a dll can be mapped anywhere
# (SE 1.5.97, InventoryLagFix.dll + 0x2B70, 1.5.78). The five-byte branch is written by
# this plugin itself: CommonLib write_branch routes it through a slot next to the
# executable and ends the process when the site is out of that slot's reach, which killed
# the game at startup on AE 1.6.1170 (1.5.79, "displacement is out of range").
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
Assert-Contains $support 'IsStackStoreModrm' 'only stack stores of a register are accepted in a copied prologue'
Assert-Contains $support 'at[1] == 0x89' 'the stack stores of mov are recognised by their opcode'
Assert-Contains $support 'at[1] >= 0x50' 'pushes of volatile registers are allowed'
Assert-Contains $support 'length > left' 'an instruction that does not end on the copied boundary is refused'
Assert-Contains $support 'bool Readable(' 'the addresses a jump chain passes through are validated before they are read'
Assert-Contains $support 'bool Executable(' 'the code a jump chain ends at has to be executable'
Assert-Contains $support 'std::uintptr_t FollowEntryJumps(const char* a_hook, std::uintptr_t a_entry, std::uintptr_t* a_firstJump)' 'the entry jump chain is followed'
Assert-Contains $support 'bytes[0] == 0xE9' 'a five-byte jump at an entry is recognised'
Assert-Contains $support 'bytes[0] == 0xFF && bytes[1] == 0x25' 'an absolute jump at an entry is recognised'
Assert-Contains $support 'bool BranchInRange(' 'the reach of the five-byte branch is checked'
Assert-Contains $support 'std::uintptr_t AllocateFarStub(const char* a_hook, std::uintptr_t a_site, std::uintptr_t a_target)' 'code out of reach of the trampoline is reached through a stub next to the site'
Assert-Contains $support 'bytes[0] = 0xFF;' 'the stub is an absolute jump that needs no relative displacement'
Assert-Contains $support 'bytes[1] = 0x25;' 'the stub jumps through a pointer, so it touches no register and no stack slot'
Assert-Contains $support 'MEM_RESERVE | MEM_COMMIT' 'the stub page is reserved next to the site'
Assert-Contains $support '::VirtualProtect(stub, kCommit, PAGE_EXECUTE_READ, &previous);' 'the stub is handed over read-execute once written'
Assert-Contains $support '::FlushInstructionCache(::GetCurrentProcess(), stub, kFarStubSize);' 'the stub is flushed before the branch to it is written'
Assert-Contains $support 'bool WriteBranchTo(const char* a_hook, std::uintptr_t a_site, std::uintptr_t a_target)' 'the five-byte branch is written by this plugin itself'
Assert-Contains $support 'bytes[0] = 0xE9;// jmp rel32' 'the branch written into a site is a five-byte relative jump'
Assert-Contains $support '::VirtualProtect(pageStart, pageSpan, PAGE_EXECUTE_READWRITE, &previous)' 'the page of the site is made writable for the length of the write only'
Assert-Contains $support '::FlushInstructionCache(::GetCurrentProcess(), reinterpret_cast<const void*>(a_site), kLength);' 'the patched site is flushed after the branch is written'
Assert-Contains $supportH 'std::uintptr_t FollowEntryJumps(' 'the entry jump chain helper is declared'
Assert-Contains $supportH 'bool BranchInRange(' 'the branch reach helper is declared'
Assert-Contains $supportH 'std::uintptr_t AllocateFarStub(' 'the far stub helper is declared'
Assert-Contains $supportH 'inline constexpr std::size_t kFarStubSize = 14;' 'the size of the far stub is one constant'
Assert-Contains $supportH 'bool WriteBranchTo(const char* a_hook, std::uintptr_t a_site, std::uintptr_t a_target);' 'the branch writer is declared next to the stub helper'
Assert-Contains $supportH 'std::string ModuleOwner(' 'the module owner helper is declared'

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
Assert-Contains $customBlock 'FollowEntryJumps("CustomSkinPlayer", declared)' 'a hook already placed at the entry by another plugin is chained instead of giving up'
Assert-Absent $customBlock 'const auto entry = InventoryChanges_ExecuteVisitorOnWorn.address();' 'the entry is no longer hooked without looking at what sits there'
Assert-Contains $hooks 'auto branchTarget = reinterpret_cast<std::uintptr_t>(code);' 'the branch target is what the site is patched with'
Assert-Contains $hooks 'branchTarget = AllocateFarStub(a_name, a_site, branchTarget);' 'a site out of reach of the trampoline is bridged with a far stub instead of disabling the hook'
Assert-Contains $hooks 'if (!WriteBranchTo(a_name, a_site, branchTarget)) {' 'the five-byte branch is written by this plugin, not routed through the trampoline slot of CommonLib'
Assert-Absent $hooks 'g_branchTrampoline->write_branch<5>' 'no hook goes through write_branch, whose slot next to the executable ends the process for a far site (1.5.79, AE 1.6.1170: displacement is out of range)'
Assert-Contains $hooks 'WriteBranchTo("RTTIPrinter", InventoryChanges_ExecuteVisitorOnWorn_Hook' 'the debug RTTI hook writes its branch the same way'
Assert-Contains $hooks 'ShowIllusionUnavailable();' 'a session without the hooks says so in a box at startup instead of looking healthy'
Assert-Contains $hooks '::LoadLibraryW(L"user32.dll")' 'the box is resolved at run time, so the plugin gains no import dependency'
Assert-Absent $hooks 'is out of reach of the trampoline stub; this hook stays disabled so the game keeps running.' 'the hook is no longer given up when the site is far away'
Assert-Absent $customBlock 'FindWornPassCall(' 'the call site inside the game routine is no longer scanned'
Assert-Absent $customBlock 'the display set will be attached from the deferred pass' 'a missing display half is no longer advertised as a working deferred pass'
Assert-Absent $customBlock 'mov(rdx, rdi);' 'the stub no longer trusts rdi (AE holds a different object there)'
Assert-Absent $customBlock 'mov(rdx, r15);' 'the stub no longer trusts r15'
Assert-Contains $customBlock 'jmp(ptr[rip]);' 'the trampoline jumps back into the game function'

$patches = Read-RepoFile 'src/cpp/hooking/Patches.cpp'
$attachBlock = Get-Block $patches 'void CustomFromWornPass(' '// Called from the main thread, outside any skinning pass.' 'CustomFromWornPass'
Assert-Contains $attachBlock 'PassBiped(a_visitor)' 'the pass is identified by the visitor it was handed'
Assert-Contains $attachBlock 'LogForeignVisitor(a_visitor);' 'a pass this build does not know is named instead of being left alone silently'
Assert-Contains $attachBlock 'the worn pass hook has been reached' 'the first hit is reported, so a log tells the hook apart from a hook that never fires'
Assert-Contains $patches 'std::atomic_bool g_wornPassHitLogged{ false };' 'the first hit is announced once'
Assert-Contains $patches 'the hooked worn pass was handed a visitor with vtable' 'an unexpected visitor vtable is logged'
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
if ($version -match 'SKYRIMOUTFITSYSTEMNG_VERSION_PATCH\s+(\d+)') {
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
