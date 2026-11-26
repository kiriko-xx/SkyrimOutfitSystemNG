//
// Skinning hooks, single implementation for every supported runtime.
//
// Every game address is declared once as REL::RelocationID(seID, aeID) - the
// CommonLibSSE-NG address table. There is no second copy of the hook code and no
// per-runtime offset table: the address library says which function a hook lives
// in, FindCallInFunction locates the call inside that function, and the trampoline
// is written there. A runtime that recompiled the function only moves the site -
// the hook finds it again instead of disabling itself.
//
// Only where the *injected machine code* itself differs does the trampoline branch
// on REL::Module::IsAE():
//   * DontVanillaSkinPlayer reads the actor from the register the skinning routine
//     keeps it in (r13) while it restores the argument registers for its own call;
//   * FixSkillLeveling targets a function that was inlined into its caller in AE,
//     so the trampoline needs a different prologue and a different exit.
//

#include "hooking/Hooks.hpp"

#include <cstdint>
#include <string_view>

#include <xbyak/xbyak.h>

#include "hooking/HookSupport.h"
#include "hooking/Patches.hpp"

namespace Hooking {
    namespace {
        void LogResolved(std::string_view a_name, const REL::RelocationID& a_id) {
            LOG(debug, "{} = {:x}", a_name, a_id.address() - REL::Module::get().base());
        }

        // Allocates the trampoline bytes next to the game code and patches the site
        // with a 5-byte branch. Templated so each generator keeps its own type.
        template <class Code>
        bool Install(const char* a_name, std::uintptr_t a_site, Code& a_code) {
            // A full trampoline aborts the process inside do_allocate, so check first and
            // leave the hook uninstalled instead.
            if (g_localTrampoline->free_size() < a_code.getSize()) {
                LOG(critical, "{}: the trampoline has {} free bytes and the hook needs {}; this hook stays disabled.", a_name, g_localTrampoline->free_size(), a_code.getSize());
                return false;
            }
            void* code = g_localTrampoline->allocate(a_code);
            if (!code) {
                LOG(critical, "{}: the trampoline is full; this hook stays disabled.", a_name);
                return false;
            }
            // A hook may end up on code a plugin owns rather than on the game function
            // itself, and that code can sit anywhere: the trampoline sits next to the game
            // executable, while a dll is mapped wherever Windows finds room. CommonLib ends
            // the process when the five-byte branch does not reach, so 1.5.78 refused the
            // hook in that case - and on SE 1.5.97, where another plugin had hooked the worn
            // visitor first, the display half was missing and the illusion stayed off for the
            // whole session. The distance is bridged with a stub next to the site instead, so
            // the hook works no matter where the code it patches is mapped.
            auto branchTarget = reinterpret_cast<std::uintptr_t>(code);
            if (!BranchInRange(a_site, branchTarget)) {
                branchTarget = AllocateFarStub(a_name, a_site, branchTarget);
                if (branchTarget == 0 || !BranchInRange(a_site, branchTarget)) {
                    LOG(critical, "{}: the hook site {} is out of reach of the trampoline stub and no jump stub could be placed next to it; this hook stays disabled so the game keeps running.", a_name, ModuleOwner(a_site));
                    return false;
                }
            }

            LOG(debug, "Patching {} at addr = {:x}. base = {:x}", a_name, a_site, REL::Module::get().base());
            g_branchTrampoline->write_branch<5>(a_site, branchTarget);
            LOG(debug, "{}: patched.", a_name);
            return true;
        }

        // Allocates trampoline code without patching anything. Used for the prologue copy
        // an entry hook calls before it jumps back into the game function.
        template <class Code>
        void* AllocateCode(const char* a_name, Code& a_code) {
            if (g_localTrampoline->free_size() < a_code.getSize()) {
                LOG(critical, "{}: the trampoline has {} free bytes and the code needs {}; this hook stays disabled.", a_name, g_localTrampoline->free_size(), a_code.getSize());
                return nullptr;
            }
            void* code = g_localTrampoline->allocate(a_code);
            if (!code) {
                LOG(critical, "{}: the trampoline is full; this hook stays disabled.", a_name);
            }
            return code;
        }
    }// namespace

    namespace Detail {
        // The two hooks that have to work as a pair: DontVanillaSkinPlayer suppresses
        // the vanilla apply pass, CustomSkinPlayer attaches the display set in its
        // place. With either one missing the actor would end up with no armour at all,
        // so the illusion stays switched off unless both are installed.
        bool g_dontVanillaSkinPlayerInstalled = false;
        bool g_customSkinPlayerInstalled = false;

        namespace DontVanillaSkinPlayer {
            using Hooking::DontVanillaSkinPlayer::ShouldOverride;

            // TESObjectARMO::ApplyArmorAddon is called for the player by the vanilla
            // skinning routine; we skip it while an outfit override is active.
            REL::RelocationID DontVanillaSkinPlayer_Hook_ID(24232, 24736);
            REL::RelocationID TESObjectARMO_ApplyArmorAddon(17392, 17792);

            std::uintptr_t DontVanillaSkinPlayer_Hook = 0;

            void Apply() {
                if (!RequireIds("DontVanillaSkinPlayer", { DontVanillaSkinPlayer_Hook_ID, TESObjectARMO_ApplyArmorAddon })) {
                    return;
                }

                // What the site looks like, independent of the address it sits at: the
                // routine fills the call arguments right before the call (r9d = race flag,
                // rdx = the item list, rcx = the body model) and turns the call result into
                // the "armour applied" flag right after it. The next function along calls the
                // same function and ends in a return straight after, so the two never match
                // the same fingerprints. The register the flag is written through differs
                // between the runtimes (dil on SE, sil on AE), hence the wildcards.
                const SiteShape shape{
                    "vanilla armour pass",
                    {
                        { -18, "44 8B 49 38 41 83 E1 01 48 8B 91 58 01 00 00 48 8B CD" },
                        { 5, "40 B? 01 40 0F B6 C?" },
                    }
                };
                DontVanillaSkinPlayer_Hook = FindVerifiedCall("DontVanillaSkinPlayer", DontVanillaSkinPlayer_Hook_ID, TESObjectARMO_ApplyArmorAddon, shape);
                if (!DontVanillaSkinPlayer_Hook) {
                    return;
                }

                LOG(debug, "Patching vanilla player skinning");
                LogResolved("TESObjectARMO_ApplyArmorAddon", TESObjectARMO_ApplyArmorAddon);

                struct Code : Xbyak::CodeGenerator {
                    Code(std::uintptr_t a_resume, std::uintptr_t a_applyArmorAddon) {
                        Xbyak::Label j_Out;
                        Xbyak::Label f_ApplyArmorAddon;
                        Xbyak::Label f_ShouldOverride;

                        // armor in rcx, target in r13
                        push(rcx);
                        push(rdx);
                        push(r9);
                        push(r8);
                        mov(rdx, r13);
                        sub(rsp, 0x40);
                        call(ptr[rip + f_ShouldOverride]);
                        add(rsp, 0x40);
                        pop(r8);
                        pop(r9);
                        pop(rdx);
                        pop(rcx);
                        test(al, al);
                        jnz(j_Out);
                        call(ptr[rip + f_ApplyArmorAddon]);
                        L(j_Out);
                        jmp(ptr[rip]);
                        dq(a_resume);

                        L(f_ApplyArmorAddon);
                        dq(a_applyArmorAddon);

                        L(f_ShouldOverride);
                        dq(uintptr_t(ShouldOverride));
                    }
                };

                Code gen{ DontVanillaSkinPlayer_Hook + 0x5, TESObjectARMO_ApplyArmorAddon.address() };
                g_dontVanillaSkinPlayerInstalled = Install("vanilla player skinning", DontVanillaSkinPlayer_Hook, gen);
            }
        }// namespace DontVanillaSkinPlayer

        namespace ShimWornFlags {
            using Hooking::ShimWornFlags::OverrideWornFlags;

            REL::RelocationID ShimWornFlags_Hook_ID(24220, 24724);
            REL::RelocationID InventoryChanges_GetWornMask(15806, 16044);

            std::uintptr_t ShimWornFlags_Hook = 0;

            void Apply() {
                if (!RequireIds("ShimWornFlags", { ShimWornFlags_Hook_ID, InventoryChanges_GetWornMask })) {
                    return;
                }

                // The inventory is the one the routine keeps in rax and locks right before
                // the call; only the register the lock copy names differs between runtimes.
                const SiteShape shape{
                    "worn mask call",
                    {
                        { -8, "48 8B C8 4C 89 ?? 24 70" },
                    }
                };
                ShimWornFlags_Hook = FindVerifiedCall("ShimWornFlags", ShimWornFlags_Hook_ID, InventoryChanges_GetWornMask, shape);
                if (!ShimWornFlags_Hook) {
                    return;
                }

                LOG(debug, "Patching shim worn flags");
                LogResolved("InventoryChanges_GetWornMask", InventoryChanges_GetWornMask);

                struct Code : Xbyak::CodeGenerator {
                    Code(std::uintptr_t a_resume, std::uintptr_t a_overrideWornFlags) {
                        Xbyak::Label f_OverrideWornFlags;

                        // The hooked instruction is a member call, so rcx is the
                        // InventoryChanges being asked for its worn mask. That is
                        // everything the C++ side needs: it finds the owner through
                        // the inventory and falls back to the vanilla mask itself.
                        // Reading the actor out of a callee-saved register would tie
                        // this hook to one register allocation of the host function.
                        push(rcx);
                        sub(rsp, 0x8);// the entry stack is 16-byte aligned, the push left it 8 off
                        sub(rsp, 0x20);
                        call(ptr[rip + f_OverrideWornFlags]);
                        add(rsp, 0x20);
                        add(rsp, 0x8);
                        pop(rcx);
                        jmp(ptr[rip]);
                        dq(a_resume);

                        L(f_OverrideWornFlags);
                        dq(a_overrideWornFlags);
                    }
                };

                Code gen{ ShimWornFlags_Hook + 0x5, std::uintptr_t(OverrideWornFlags) };
                Install("shim worn flags", ShimWornFlags_Hook, gen);
            }
        }// namespace ShimWornFlags

        namespace CustomSkinPlayer {
            using Hooking::CustomSkinPlayer::CustomFromWornPass;
            using Hooking::CustomSkinPlayer::SetSiteHookInstalled;
            using Hooking::CustomSkinPlayer::SetWornVisitorVTable;

            // The function every skinning pass calls to walk an actor's worn items, hooked
            // at its own entry. Hooking the callee instead of one of the calls inside its
            // caller is what makes this hook reachable on AE 1.6.1170: the address library
            // gives the function itself, which no other plugin can move, while the calls
            // inside the skinning routine belong to whoever patched them last. On 1.6.1170
            // only one of the two calls to this function still reads as a plain call at run
            // time, and that one is on the pass which is skipped, so 1.5.56 to 1.5.60
            // attached nothing while the real armour was already hidden.
            REL::RelocationID InventoryChanges_ExecuteVisitorOnWorn(15856, 16096);
            // The vtable of the visitor the pass that fills the biped is handed. The checker
            // pass of AE 1.6.1170 hands a different object, so the visitor itself - not the
            // order of the calls - tells the two passes apart.
            REL::RelocationID WornVisitorVTable(241890, 195851);

            void Apply() {
                if (!RequireIds("CustomSkinPlayer", { InventoryChanges_ExecuteVisitorOnWorn })) {
                    return;
                }
                LogResolved("InventoryChanges_ExecuteVisitorOnWorn", InventoryChanges_ExecuteVisitorOnWorn);
                // Tells the C++ side which visitor object marks the filling pass, so the
                // BipedAnim being filled is read out of the visitor instead of a register.
                if (IdExists(WornVisitorVTable)) {
                    SetWornVisitorVTable(WornVisitorVTable.address());
                } else {
                    LOG(critical, "CustomSkinPlayer: this build's address library has no entry for the visitor of the skinning pass, so the display half cannot tell the filling pass from the checking one; the illusion stays off for this session and the real armour shows.");
                }

                const auto declared = InventoryChanges_ExecuteVisitorOnWorn.address();
                // Another plugin may already have hooked this function, and then its entry
                // reads as a jump into that plugin's detour instead of the prologue of the
                // game function. Chaining on top of it keeps the display half working;
                // 1.5.64 and earlier gave up here and left the illusion off for the whole
                // session (an SE 1.5.97 report showed E9 2B AE D7 FF at the entry where the
                // shipped executable has 48 89 5C 24 10).
                const auto entry = FollowEntryJumps("CustomSkinPlayer", declared);
                if (!entry) {
                    LOG(err, "CustomSkinPlayer: the worn visitor at {} could not be reached through the code that sits at its entry; the hook stays disabled.", ModuleOwner(declared));
                    LogSiteBytes("CustomSkinPlayer", declared);
                    return;
                }
                constexpr std::size_t kStolen = 5;
                const auto copied = CopyableEntryLength(entry, kStolen);
                if (copied != kStolen) {
                    LOG(err, "CustomSkinPlayer: the entry of the worn visitor at {} starts with a sequence this hook cannot copy into a trampoline; the hook stays disabled.", ModuleOwner(entry));
                    LogSiteBytes("CustomSkinPlayer", entry);
                    return;
                }

                // The copied prologue of the game function, followed by a jump to the rest of
                // it: the injected code calls this, so the vanilla pass still runs exactly
                // once with the arguments and the stack the game prepared for it.
                struct Prologue : Xbyak::CodeGenerator {
                    Prologue(std::uintptr_t a_entry, std::size_t a_copied) {
                        db(reinterpret_cast<const std::uint8_t*>(a_entry), a_copied);
                        jmp(ptr[rip]);
                        dq(a_entry + a_copied);
                    }
                };

                Prologue pro{ entry, copied };
                void* trampoline = AllocateCode("custom skin player prologue", pro);
                if (!trampoline) {
                    return;
                }

                // Entry stub. The inventory (rcx) and the visitor (rdx) are still the ones the
                // game passed, so nothing here depends on a register a runtime happens to keep
                // the actor or the weight model in. Order matters: the vanilla pass runs first
                // and leaves the real armour in the biped, then the display set is written over
                // it, exactly like the SE hook does. 1.5.57 took the model from r15 and 1.5.60
                // wrote the biped from outside the pass; neither ever produced geometry.
                struct Code : Xbyak::CodeGenerator {
                    Code(std::uintptr_t a_trampoline, std::uintptr_t a_customFromWornPass) {
                        Xbyak::Label f_CustomFromWornPass;
                        Xbyak::Label f_trampoline;

                        push(rcx);
                        push(rdx);
                        sub(rsp, 0x28);// shadow space for the callee, keeps rsp 16-byte aligned
                        call(ptr[rip + f_trampoline]);
                        add(rsp, 0x28);

                        mov(rcx, ptr[rsp + 8]);
                        mov(rdx, ptr[rsp]);
                        sub(rsp, 0x28);
                        call(ptr[rip + f_CustomFromWornPass]);
                        add(rsp, 0x28);

                        pop(rdx);
                        pop(rcx);
                        ret();

                        L(f_CustomFromWornPass);
                        dq(a_customFromWornPass);

                        L(f_trampoline);
                        dq(a_trampoline);
                    }
                };

                Code gen{ reinterpret_cast<std::uintptr_t>(trampoline), std::uintptr_t(CustomFromWornPass) };
                g_customSkinPlayerInstalled = Install("custom skin player", entry, gen);
                // Tells the C++ side whether the display set is attached from inside the
                // skinning pass or from the deferred pass it drives itself.
                SetSiteHookInstalled(g_customSkinPlayerInstalled);
                if (g_customSkinPlayerInstalled) {
                    LOG(info, "CustomSkinPlayer: the worn visitor is hooked at {} (declared at {}), so the display set is attached inside the pass that fills the biped.", ModuleOwner(entry), ModuleOwner(declared));
                } else {
                    // No deferred pass can stand in for this hook: with the display half
                    // missing the pair is incomplete, the illusion is switched off below and
                    // the player sees the real armour. Say so instead of pointing at a pass
                    // that would produce unfinished parts (1.5.22).
                    LOG(critical, "CustomSkinPlayer: the worn visitor of this game build could not be hooked, so the display set cannot be attached inside the pass that fills the biped; the illusion stays off for this session and the real armour shows. The reason is in the lines above.");
                }
            }
        }// namespace CustomSkinPlayer

        namespace FixEquipConflictCheck {
            using Hooking::FixEquipConflictCheck::Inner;
            using Hooking::FixEquipConflictCheck::ShouldOverride;

            REL::RelocationID FixEquipConflictCheck_Hook_ID(36979, 38004);
            REL::RelocationID BGSBipedObjectForm_TestBodyPartByIndex(14026, 14119);

            std::uintptr_t FixEquipConflictCheck_Hook = 0;

            void Apply() {
                if (!RequireIds("FixEquipConflictCheck", { FixEquipConflictCheck_Hook_ID, BGSBipedObjectForm_TestBodyPartByIndex })) {
                    return;
                }

                // The per-slot conflict test is called with the slot index in edx and the
                // body form in rcx, and its verdict is used as "this item conflicts". Both
                // halves of that exist only in this loop: the two other calls to the same
                // function in the scan window pass different registers.
                const SiteShape shape{
                    "equip conflict loop",
                    {
                        { -8, "4C 8B F0 8B D3 48 8B CD" },
                        { 5, "84 C0 0F 84" },
                    }
                };
                FixEquipConflictCheck_Hook = FindVerifiedCall("FixEquipConflictCheck", FixEquipConflictCheck_Hook_ID, BGSBipedObjectForm_TestBodyPartByIndex, shape);
                if (!FixEquipConflictCheck_Hook) {
                    return;
                }

                LOG(debug, "Patching fix for equip conflict check");
                LogResolved("BGSBipedObjectForm_TestBodyPartByIndex", BGSBipedObjectForm_TestBodyPartByIndex);

                struct Code : Xbyak::CodeGenerator {
                    Code(std::uintptr_t a_resume, std::uintptr_t a_testBodyPartByIndex) {
                        Xbyak::Label j_Out;
                        Xbyak::Label j_Exit;
                        Xbyak::Label f_Inner;
                        Xbyak::Label f_TestBodyPartByIndex;
                        Xbyak::Label f_ShouldOverride;

                        call(ptr[rip + f_TestBodyPartByIndex]);
                        test(al, al);
                        jz(j_Exit);

                        // rsp+0x10: item
                        // rdi: Actor
                        // rbx: Body Slot
                        push(rcx);
                        // Set RCX to the RDX argument of the patched function
                        // RSP + Argument offset rel to original entry + Offset from push above
                        // + Offset from pushes in original entry
                        mov(rcx, ptr[rsp + 0x10 + 0x08 + 0xC8]);
                        sub(rsp, 0x8);// Ensure 16-byte alignment of stack pointer
                        sub(rsp, 0x20);
                        call(ptr[rip + f_ShouldOverride]);
                        add(rsp, 0x20);
                        add(rsp, 0x8);
                        pop(rcx);
                        test(al, al);
                        mov(rax, 1);
                        jz(j_Out);
                        push(rcx);
                        push(rdx);
                        mov(rcx, rbx);
                        mov(rdx, rdi);
                        sub(rsp, 0x20);
                        call(ptr[rip + f_Inner]);
                        add(rsp, 0x20);
                        pop(rdx);
                        pop(rcx);

                        L(j_Exit);
                        xor_(al, al);

                        L(j_Out);
                        jmp(ptr[rip]);
                        dq(a_resume);

                        L(f_TestBodyPartByIndex);
                        dq(a_testBodyPartByIndex);

                        L(f_ShouldOverride);
                        dq(uintptr_t(ShouldOverride));

                        L(f_Inner);
                        dq(uintptr_t(Inner));
                    }
                };

                Code gen{ FixEquipConflictCheck_Hook + 0x5, BGSBipedObjectForm_TestBodyPartByIndex.address() };
                Install("fix equip conflict check", FixEquipConflictCheck_Hook, gen);
            }
        }// namespace FixEquipConflictCheck

        namespace FixSkillLeveling {
            using Hooking::FixSkillLeveling::Inner;

            // 1.5.x: the visitor lives in the function body, one call away.
            // 1.6.x: that function was inlined, so we hook the mutation routine
            // itself and rebuild the visitor from the registers it keeps it in.
            REL::RelocationID SkillMutationFunction_Hook_ID(37589, 38627);
            REL::RelocationID BipedAnim_VisitGetWornArmorTypeVisitor(37688, 0);// SE only
            REL::RelocationID BipedAnim_GetActorRaceBodyArmorPtrPtr(0, 15696);// AE only

            std::uintptr_t SkillMutationFunction_Hook = 0;

            void Apply() {
                const bool isAE = REL::Module::IsAE();
                const REL::RelocationID& visitor = isAE ? BipedAnim_GetActorRaceBodyArmorPtrPtr : BipedAnim_VisitGetWornArmorTypeVisitor;

                if (!RequireIds("FixSkillLeveling", { SkillMutationFunction_Hook_ID }) ||
                    !RequireIds("FixSkillLeveling", { visitor })) {
                    return;
                }

                // SE calls a named visitor; AE inlined it into the loop and keeps the visit
                // state in registers, so the two look nothing alike. Each runtime gets the
                // fingerprints of the call it was written against.
                const SiteShape shape = isAE
                    ? SiteShape{
                          "skill mutation loop (AE)",
                          {
                              { -6, "4C 8B E8 48 8B 0E" },
                              { 5, "48 8B D8 4C 8B 36 49 83 C6 10 45 8D 67 2A" },
                          } }
                    : SiteShape{
                          "skill mutation call (SE)",
                          {
                              { -10, "48 8D 54 24 20 48 89 44 24 28" },
                              { 5, "8B 7C 24 34 8B 74 24 30 03 FE" },
                          } };
                SkillMutationFunction_Hook = FindVerifiedCall("FixSkillLeveling", SkillMutationFunction_Hook_ID, visitor, shape);
                if (!SkillMutationFunction_Hook) {
                    return;
                }

                LOG(debug, "Patching fix for skill level");
                LogResolved(isAE ? "BipedAnim_GetActorRaceBodyArmorPtrPtr" : "BipedAnim_VisitGetWornArmorTypeVisitor", visitor);

                struct Code : Xbyak::CodeGenerator {
                    Code(std::uintptr_t a_resume, std::uintptr_t a_skip,
                        std::uintptr_t a_visitor, bool a_isAE) {
                        Xbyak::Label f_Inner;
                        Xbyak::Label f_Visitor;
                        Xbyak::Label j_ResumeNormally;
                        Xbyak::Label j_SkipLoop;

                        if (a_isAE) {
                            // In the original code, the Visitor lives exclusively
                            // in the registers. The mapping is
                            //   RE::TESObjectARMO** shield = r13
                            //   RE::TESObjectARMO** torso = rbx
                            //   std::uint32_t light = r15d (32-bit)
                            //   std::uint32_t heavy = ebp (32-bit)
                            // All these registers are non-volatile
                            push(rcx);// 8
                            // We now push the register data onto the stack, laid out as the struct would be.
                            sub(rsp, 0x4);      // Fake a push of the ebp
                            mov(ptr[rsp], ebp); // 4
                            sub(rsp, 0x4);      // Fake a push of the r15d
                            mov(ptr[rsp], r15d);// 4
                            push(rbx);          // 8
                            push(r13);          // 8
                            // We don't need to fixup rsp because the pushes above should leave it 16-byte aligned
                            // rcx is already the right value, so just set rdx to point to the data we pushed
                            mov(rdx, rsp);// rsp points to the start of the pushed data
                            sub(rsp, 0x20);
                            call(ptr[rip + f_Inner]);
                            add(rsp, 0x20);
                            pop(r13);
                            pop(rbx);
                            mov(r15d, ptr[rsp]);// Fake a pop of r15d
                            add(rsp, 0x4);
                            mov(ebp, ptr[rsp]);// Fake a pop of ebp
                            add(rsp, 0x4);
                            pop(rcx);
                            test(al, al);
                            jnz(j_SkipLoop);
                            call(ptr[rip + f_Visitor]);
                        } else {
                            push(rcx);
                            push(rdx);
                            sub(rsp, 0x20);
                            call(ptr[rip + f_Inner]);
                            add(rsp, 0x20);
                            pop(rdx);
                            pop(rcx);
                            test(al, al);
                            jnz(j_ResumeNormally);
                            call(ptr[rip + f_Visitor]);
                        }

                        L(j_ResumeNormally);
                        jmp(ptr[rip]);
                        dq(a_resume);

                        if (a_isAE) {
                            L(j_SkipLoop);
                            mov(r12, 0x0);
                            jmp(ptr[rip]);
                            dq(a_skip);
                        }

                        L(f_Visitor);
                        dq(a_visitor);

                        L(f_Inner);
                        dq(uintptr_t(Inner));
                    }
                };

                // The SE branch resumes right after the patched call; the AE branch either
                // returns to the caller or jumps past the vanilla loop. Where the loop ends
                // is a property of the loop, not a constant: it is the instruction behind the
                // branch that closes it, so it is derived from the code instead of recorded
                // (on 1.6.1170 that instruction sat 123 bytes past the call, which is the value the
                // old code had written down; the number itself is not trusted, only the derivation).
                std::uintptr_t skip = SkillMutationFunction_Hook + 0x5;
                if (isAE && !DeriveLoopSkip("FixSkillLeveling", SkillMutationFunction_Hook_ID.address(), SkillMutationFunction_Hook, &skip)) {
                    return;
                }
                Code gen{ SkillMutationFunction_Hook + 0x5, skip, visitor.address(), isAE };
                Install("skill level logic", SkillMutationFunction_Hook, gen);
            }
        }// namespace FixSkillLeveling

        namespace RTTIPrinter {
            using Hooking::RTTIPrinter::Print_RTTI;

            REL::RelocationID InventoryChanges_ExecuteVisitorOnWorn(0, 16096);// AE only

            std::uintptr_t InventoryChanges_ExecuteVisitorOnWorn_Hook = 0;

            void Apply() {
                if (!REL::Module::IsAE() || !RequireIds("RTTIPrinter", { InventoryChanges_ExecuteVisitorOnWorn })) {
                    return;
                }

                InventoryChanges_ExecuteVisitorOnWorn_Hook = ResolveSite("RTTIPrinter", InventoryChanges_ExecuteVisitorOnWorn, 0x2A);
                if (!InventoryChanges_ExecuteVisitorOnWorn_Hook) {
                    return;
                }

                LOG(debug, "Patching RTTI print");

                struct Code : Xbyak::CodeGenerator {
                    Code(std::uintptr_t a_resume) {
                        Xbyak::Label f_PrintRTTI;

                        push(rcx);
                        push(rdx);
                        push(rax);
                        mov(rcx, rdx);
                        sub(rsp, 0x8);// Ensure 16-byte alignment of stack pointer
                        sub(rsp, 0x20);
                        call(ptr[rip + f_PrintRTTI]);
                        add(rsp, 0x20);
                        add(rsp, 0x8);
                        pop(rax);
                        pop(rdx);
                        pop(rcx);

                        jmp(ptr[rip]);
                        dq(a_resume);

                        L(f_PrintRTTI);
                        dq(uintptr_t(Print_RTTI));
                    }
                };

                // The site is a 5-byte call; resuming at +6 would land inside the next
                // instruction.
                Code gen{ InventoryChanges_ExecuteVisitorOnWorn_Hook + 0x5 };
                void* code = g_localTrampoline->allocate(gen);
                if (!code) {
                    LOG(critical, "RTTIPrinter: the trampoline is full; this hook stays disabled.");
                    return;
                }
                g_branchTrampoline->write_branch<5>(InventoryChanges_ExecuteVisitorOnWorn_Hook, code);
            }
        }// namespace RTTIPrinter
    }// namespace Detail

    void ApplyPlayerSkinningHooks() {
        LOG(debug, "Installing skinning hooks for runtime {}.", REL::Module::get().version().string());
        LogModuleIdentity("Skinning hooks");
        Detail::DontVanillaSkinPlayer::Apply();
        Detail::ShimWornFlags::Apply();
        Detail::CustomSkinPlayer::Apply();
        Detail::FixEquipConflictCheck::Apply();
        Detail::FixSkillLeveling::Apply();
#if _DEBUG
        Detail::RTTIPrinter::Apply();
#endif

        // Hiding the real armour without something to put in its place shows up as a
        // nude actor, so the illusion stays off unless both halves are in place. Up to
        // 1.5.19 the display half was the CustomSkinPlayer hook; 1.5.22 tried to keep
        // the illusion on with only the hiding hook and let the deferred pass attach
        // the display set, and users saw a nude actor, so both are required again.
        const bool illusionAvailable = Detail::g_dontVanillaSkinPlayerInstalled && Detail::g_customSkinPlayerInstalled;
        SetIllusionDisplayEnabled(illusionAvailable);
        if (illusionAvailable) {
            LOG(info, "Illusion display is active: the player skinning hooks are installed.");
        } else {
            LOG(critical, "Illusion display is off for this session: the player skinning hooks are not both installed (vanilla pass {}, display pass {}). The real equipped armour stays visible. Nothing else is disabled.",
                Detail::g_dontVanillaSkinPlayerInstalled ? "hooked" : "missing",
                Detail::g_customSkinPlayerInstalled ? "hooked" : "missing");
        }
    }
}// namespace Hooking
