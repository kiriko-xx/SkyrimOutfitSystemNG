$ErrorActionPreference = 'Stop'
# Static guard for the two skinning hooks the illusion rests on (1.5.89). One hides, one
# attaches, and they sit on different functions:
#
#   * TESObjectARMO::ApplyArmorAddon, at its entry: the game's own arguments (armour, weight
#     model) are handed to the decision and a piece is never hidden when it is the skin;
#   * InventoryChanges::ExecuteVisitorOnWorn, at its entry: the vanilla pass runs first and
#     then the display set is written over the biped it filled, because the routine that
#     CALLS that pass is what turns the slots into geometry. Writing the biped from anywhere
#     else (1.5.82 inside ApplyArmorAddon, 1.5.87 and 1.5.88 from a deferred queue) claims
#     slots that never get a mesh: in the 2026-11-27 log every one of 368 slot lines read
#     "part 1 clone 0", an empty preset drew a head only, and a claimed hair slot left the
#     actor bald.
#
# Both entries are checked against the prologue they were written for
# before anything is written, a prologue that is safe to relocate is copied, the stub reads
# no register the engine happens to hold, the five-byte branch is written by this plugin
# itself (CommonLib write_branch routes it through a slot next to the executable and ends
# the process when the site is out of that slot's reach: 1.5.79, "displacement is out of
# range"), and a site another plugin moved out of the executable is reached through a stub
# next to the site (SE 1.5.97, InventoryLagFix.dll + 0x2B70, 1.5.78). The worn visitor is
# hooked through that same path, so a build where another plugin owns the function still
# gets the display set.
#
# ---- the armour pass hides, the worn pass attaches ----
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
    if ($start -lt 0) { throw "cannot locate $label" }
    $end = $text.IndexOf($endMarker, $start)
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
$armourBlock = Get-Block $hooks 'namespace DontVanillaSkinPlayer {' 'namespace ShimWornFlags' 'the armour pass hook block'
Assert-Contains $hooks 'REL::RelocationID TESObjectARMO_ApplyArmorAddon(17392, 17792);' 'the armour pass is addressed through the address library, at its entry'
Assert-Contains $armourBlock 'inline constexpr std::uint8_t kArmourPassPrologue[]' 'the prologue of the armour pass is pinned in the code'
Assert-Contains $armourBlock 'EntryMatches("DontVanillaSkinPlayer", entry, kArmourPassPrologue, sizeof(kArmourPassPrologue))' 'the entry is checked against that prologue before anything is written'
Assert-Contains $armourBlock 'CopyableEntryLength(entry, kStolen)' 'the prologue is checked before it is copied'
Assert-Contains $armourBlock 'mov(rdx, r8);' 'the weight model the game passes is handed to the decision'
Assert-Contains $armourBlock 'call(ptr[rip + f_Decide]);' 'the decision is a call into this plugin'
Assert-Contains $armourBlock 'xor_(eax, eax);' 'a hidden piece returns without putting anything on the biped'
Assert-Contains $armourBlock 'db(reinterpret_cast<const std::uint8_t*>(a_stolen), a_stolenLength);' 'the trampoline carries a copy of the game prologue'
Assert-Contains $armourBlock 'jmp(ptr[rip]);' 'the trampoline jumps back into the game function'
Assert-Contains $armourBlock 'Install("armour pass", entry, gen)' 'the hook is written to the entry of the armour pass'
Assert-Absent $armourBlock 'g_customSkinPlayerInstalled = g_dontVanillaSkinPlayerInstalled;' 'the attach half is no longer taken from the hide half'
Assert-Contains $hooks 'REL::RelocationID InventoryChanges_ExecuteVisitorOnWorn(15856, 16096);' 'the worn visitor is hooked again'
Assert-Contains $hooks 'WornVisitorVTable(241890, 195851)' 'the visitor of the filling pass is told apart from the checking one'
Assert-Contains $hooks 'namespace CustomSkinPlayer {' 'the attach half is its own hook'
Assert-Contains $hooks 'g_customSkinPlayerInstalled = Install("custom skin player", entry, gen);' 'and reports its own result'
Assert-Contains $hooks 'Detail::CustomSkinPlayer::Apply();' 'and is installed'
Assert-Absent $armourBlock 'FindVerifiedCall(' 'no call site inside the game routine is fingerprinted any more'
Assert-Absent $armourBlock 'mov(rdx, r13)' 'no register the caller happens to keep the actor in is trusted'
Assert-Absent $armourBlock 'mov(rdx, r15)' 'no register the caller happens to keep the actor in is trusted'
Assert-Contains $hooks 'auto branchTarget = reinterpret_cast<std::uintptr_t>(code);' 'the branch target is what the site is patched with'
Assert-Contains $hooks 'branchTarget = AllocateFarStub(a_name, a_site, branchTarget);' 'a site out of reach of the trampoline is bridged with a far stub instead of disabling the hook'
Assert-Contains $hooks 'if (!WriteBranchTo(a_name, a_site, branchTarget)) {' 'the five-byte branch is written by this plugin, not routed through the trampoline slot of CommonLib'
Assert-Absent $hooks 'g_branchTrampoline->write_branch<5>' 'no hook goes through write_branch, whose slot next to the executable ends the process for a far site (1.5.79, AE 1.6.1170: displacement is out of range)'
Assert-Contains $hooks 'WriteBranchTo("RTTIPrinter", InventoryChanges_ExecuteVisitorOnWorn_Hook' 'the debug RTTI hook writes its branch the same way'
Assert-Contains $hooks 'ShowIllusionUnavailableBox(true);' 'a session without the hook says so in a box at startup instead of looking healthy'
Assert-Contains $hooks '::LoadLibraryW(L"user32.dll")' 'the box is resolved at run time, so the plugin gains no import dependency'
Assert-Absent $hooks 'is out of reach of the trampoline stub; this hook stays disabled so the game keeps running.' 'the hook is no longer given up when the site is far away'

$patches = Read-RepoFile 'src/cpp/hooking/Patches.cpp'
$decideBlock = Get-Block $patches 'bool ShouldHideForModel(RE::TESObjectARMO* a_armor' '}// namespace DontVanillaSkinPlayer' 'the armour pass decision'
Assert-Contains $decideBlock 'if (g_attachDepth > 0) return false;' 'the pieces being attached are never hidden'
Assert-Contains $decideBlock 'ResolveModelOwner(a_model)' 'the actor is resolved from the weight model'
Assert-Contains $decideBlock 'if (a_armor == actor->GetSkin()) {' 'the skin is never hidden - hiding it is what left an empty preset with a head only (1.5.82)'
Assert-Contains $decideBlock 'MarkAttachPending(actor);' 'a skin-only pass asks for the pass to run again'
Assert-Absent $decideBlock 'AttachFromHook' 'nothing is attached from inside the armour pass'
Assert-Absent $decideBlock 'NotePassModel' 'no weight model is remembered for a later attach'
Assert-Contains $decideBlock 'return ShouldOverride(a_armor, actor);' 'hidden pieces are still decided by the display set'
Assert-Contains $decideBlock 'catch (...)' 'an exception inside the hook cannot take the game down'
Assert-Contains $decideBlock 'LogThrown("ShouldHideForModel");' 'and it is reported instead of passing silently'
Assert-Contains $patches 'RE::Actor* ResolveModelOwner(RE::ActorWeightModel* a_model)' 'the weight model is resolved to its owner'
Assert-Contains $patches 'auto* biped = reinterpret_cast<RE::BipedAnim* const*>(a_model)[0];' 'the biped is the first member of the weight model'
Assert-Contains $patches 'auto* owner = biped->actorRef.get().get();' 'the actor is read from the handle the engine itself keeps in the biped'
Assert-Contains $patches 'owner->formType != RE::FormType::ActorCharacter' 'the handle is checked before it is cast'
Assert-Contains $patches 'actor->GetBiped(false).get() != biped && actor->GetBiped(true).get() != biped' 'a biped that belongs to nobody is left alone'
Assert-Absent $patches 'FirstCallOfPass' 'a pass is no longer guessed from the gap between its calls'
Assert-Absent $patches 'kPassGap' 'that constant is gone with it'
Assert-Contains $patches '++g_attachDepth;' 'the attach is re-entered by its own ApplyArmorAddon calls'
Assert-Contains $patches 'RecordSiteHookHit(actor->GetFormID());' 'an actor that comes through the worn pass is recorded'
Assert-Contains $patches 'void LogThrown(const char* a_where)' 'a caught exception is counted and reported'
Assert-Contains $patches 'std::uint32_t StagedSlots(' 'the applied slots are read back'
Assert-Contains $patches 'const auto staged = StagedSlots(anim, armor);' 'a piece is judged by what landed in the biped'
Assert-Absent $patches 'if (!RE::TESObjectARMOAugments::ApplyArmorAddon' 'ApplyArmorAddon no longer gates on its return value'
Assert-Contains $patches 'RE::BipedAnim* PassBiped(RE::InventoryChanges::IItemChangeVisitor* a_visitor)' 'the biped comes out of the visitor the game handed over'
Assert-Contains $patches 'LogForeignVisitor(a_visitor);' 'an unexpected visitor is named once in the log'
Assert-Contains $patches 'void CustomFromWornPass(RE::InventoryChanges* a_inventory' 'the worn pass is where the display set is written'
Assert-Contains $patches 'void SetWornVisitorVTable(std::uintptr_t a_vtable)' 'the vtable of the filling pass is handed over'
Assert-Contains $patches 'reinterpret_cast<std::uintptr_t>(a_visitor) + 0x18' 'the biped is the visitor member at 0x18'
Assert-Absent $patches 'AttachFromKnownModel' 'nothing attaches to a remembered model'
Assert-Absent $patches 'g_passModel' 'no model is cached across passes'
Assert-Absent $patches 'ShouldOverride: Failed to cast target to Actor.' 'a cast failure no longer hides the piece it could not attribute'
Assert-Contains $patches 'SiteHookHitRecently(id)' 'the deferred attach keeps running when the hook never fires'
$drainBlock = Get-Block $patches 'void DrainAttachPending()' 'void SetSiteHookInstalled' 'DrainAttachPending'
Assert-Contains $drainBlock '::OutfitSystem::refreshArmorFor(actor);' 'the deferred pass rebuilds the equipment while the hook is installed'
Assert-Absent $drainBlock 'Custom(actor, nullptr);' 'the deferred queue never writes a biped itself'

# ---- a written branch is read back, and every site is re-checked after the load ----
# Writing five bytes says nothing about what the site holds a moment later: the next plugin
# that hooks the same function overwrites whoever came first, which used to show up only as
# an outfit that silently did not apply.
Assert-Contains $supportH 'bool VerifyBranch(' 'the written branch can be read back'
Assert-Contains $supportH 'void RegisterSite(' 'every written branch is remembered'
Assert-Contains $supportH 'std::size_t VerifyInstalledSites();' 'all written branches can be re-checked'
Assert-Contains $supportH 'void LogForeignJumps(' 'a patched call site can be reported'
Assert-Contains $support 'if (!VerifyBranch(a_hook, a_site, a_target)) {' 'writing a branch is not done before it reads back'
Assert-Contains $support 'RegisterSite(a_hook, a_site, a_target);' 'a branch that reads back is remembered'
Assert-Contains $support 'another plugin wrote over the hook site, so this hook is not in effect this session.' 'an overwritten site is reported instead of being trusted'
Assert-Contains $support 'the hook site {} now runs the detour at {}; that is the plugin which took the site over.' 'the plugin that took a site over is named'
Assert-Contains $support 'no longer in effect, so the hooks they carry do nothing this session' 'a lost site is a critical line, not a debug one'
Assert-Contains $support 'the function at {} holds a {} {} byte(s) into it that lands on {}' 'a call a foreign detour replaced is reported'
if (([regex]::Matches($support, 'LogForeignJumps\(a_hook, entry, end\);')).Count -lt 3) {
    Write-Output 'FAIL every search for a call site reports a foreign jump instead of only giving up'
    $failures++
}
$hooks = Read-RepoFile 'src/cpp/hooking/Hooks.cpp'
Assert-Absent $hooks 'g_branchTrampoline->write_branch<5>' 'no branch is written through the CommonLib slot'
$main = Read-RepoFile 'src/cpp/main.cpp'
Assert-Contains $main '#include "hooking/HookSupport.h"' 'the load order can ask whether the branches are still in place'
if (([regex]::Matches($main, 'Hooking::VerifyInstalledSites\(\);')).Count -lt 2) {
    Write-Output 'FAIL the sites are re-checked once after the load and once after a save is loaded'
    $failures++
}
$version = Read-RepoFile 'src/cpp/include/version.h'
if ($version -match 'SKYRIMOUTFITSYSTEMNG_VERSION_PATCH\s+(\d+)') {
    $patch = [int]$Matches[1]
    if ($patch -lt 85) {
        Write-Output "FAIL the single armour-pass hook arrived in 1.5.85 (found 1.5.$patch)"
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
