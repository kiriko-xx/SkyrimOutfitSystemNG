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

            LOG(debug, "Patching {} at addr = {:x}. base = {:x}", a_name, a_site, REL::Module::get().base());
            g_branchTrampoline->write_branch<5>(a_site, code);
            LOG(debug, "{}: patched.", a_name);
            return true;
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

                DontVanillaSkinPlayer_Hook = FindCallInFunction("DontVanillaSkinPlayer", DontVanillaSkinPlayer_Hook_ID, TESObjectARMO_ApplyArmorAddon);
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

                ShimWornFlags_Hook = FindCallInFunction("ShimWornFlags", ShimWornFlags_Hook_ID, InventoryChanges_GetWornMask);
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
            using Hooking::CustomSkinPlayer::Custom;
            using Hooking::CustomSkinPlayer::SetSiteHookInstalled;
            using Hooking::ShouldOverrideSkinning;

            // AE inlined the function 1.5.x called here, so the address library entry
            // differs per runtime; which call inside it belongs to the skinning pass is
            // found by scanning the function body, not by a recorded byte offset.
            REL::RelocationID CustomSkinPlayer_Hook_ID(24231, 24725);
            REL::RelocationID InventoryChanges_ExecuteVisitorOnWorn(15856, 16096);

            std::uintptr_t CustomSkinPlayer_Hook = 0;

            void Apply() {
                if (!RequireIds("CustomSkinPlayer", { CustomSkinPlayer_Hook_ID, InventoryChanges_ExecuteVisitorOnWorn })) {
                    return;
                }

                // SE 1.5.97 is the build where the recorded callee check fails while the
                // site itself still works, so this hook keeps a per-runtime fallback offset
                // (0x81 SE / 0x1EF AE - the values every release up to 1.5.19 patched).
                const std::ptrdiff_t kLegacyOffset = REL::Module::IsAE() ? 0x1EF : 0x81;
                CustomSkinPlayer_Hook = FindCallInFunctionOrOffset("CustomSkinPlayer", CustomSkinPlayer_Hook_ID, InventoryChanges_ExecuteVisitorOnWorn, kLegacyOffset);
                if (!CustomSkinPlayer_Hook) {
                    return;
                }

                LOG(debug, "Patching custom skin player");
                LogResolved("InventoryChanges_ExecuteVisitorOnWorn", InventoryChanges_ExecuteVisitorOnWorn);

                //
                // The site is the "add the armour addons to the ActorWeightModel" call of
                // the skinning routine, and the registers are part of its contract:
                // rbx holds the TESObjectREFR being skinned and rdi the ActorWeightModel
                // the vanilla pass just filled. That model has to be handed to
                // ApplyArmorAddon as-is - 1.5.20 to 1.5.23 passed the address of the
                // actor's BSTSmartPointer<BipedAnim> instead, which is a different layout,
                // so every ApplyArmorAddon call was refused and every slot stayed empty
                // (the "only head and hands" report: those are the vanilla skin parts).
                //
                struct Code : Xbyak::CodeGenerator {
                    Code(std::uintptr_t a_resume, std::uintptr_t a_executeVisitorOnWorn, std::uintptr_t a_shouldOverride, std::uintptr_t a_custom) {
                        Xbyak::Label f_Custom;
                        Xbyak::Label f_ExecuteVisitorOnWorn;
                        Xbyak::Label f_ShouldOverride;
                        Xbyak::Label j_Out;

                        // Run the vanilla pass first (it fills the model with the worn
                        // armour), then attach the outfit to the very same model.
                        push(rcx);
                        push(rdx);
                        sub(rsp, 0x20);
                        call(ptr[rip + f_ExecuteVisitorOnWorn]);
                        add(rsp, 0x20);

                        // Nothing to do for actors without an illusion; the vanilla pass
                        // has already run and the hook leaves the model untouched.
                        mov(rcx, rbx);
                        sub(rsp, 0x20);
                        call(ptr[rip + f_ShouldOverride]);
                        add(rsp, 0x20);
                        test(al, al);
                        jz(j_Out);

                        mov(rcx, rbx);
                        mov(rdx, rdi);
                        sub(rsp, 0x20);
                        call(ptr[rip + f_Custom]);
                        add(rsp, 0x20);

                        L(j_Out);
                        pop(rdx);
                        pop(rcx);
                        jmp(ptr[rip]);
                        dq(a_resume);

                        L(f_Custom);
                        dq(a_custom);

                        L(f_ExecuteVisitorOnWorn);
                        dq(a_executeVisitorOnWorn);

                        L(f_ShouldOverride);
                        dq(a_shouldOverride);
                    }
                };

                Code gen{ CustomSkinPlayer_Hook + 0x5, InventoryChanges_ExecuteVisitorOnWorn.address(), std::uintptr_t(ShouldOverrideSkinning), std::uintptr_t(Custom) };
                g_customSkinPlayerInstalled = Install("custom skin player", CustomSkinPlayer_Hook, gen);
                // Tells the C++ side whether the display set is attached from inside the
                // skinning routine or from the deferred pass it drives itself.
                SetSiteHookInstalled(g_customSkinPlayerInstalled);
                if (g_customSkinPlayerInstalled) {
                    LOG(info, "CustomSkinPlayer: the display set is attached from inside the skinning routine.");
                } else {
                    LOG(warn, "CustomSkinPlayer: the display set will be attached from the deferred pass after each skinning update, because this game build has no call site inside the skinning routine for the hook.");
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

                FixEquipConflictCheck_Hook = FindCallInFunction("FixEquipConflictCheck", FixEquipConflictCheck_Hook_ID, BGSBipedObjectForm_TestBodyPartByIndex);
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

                SkillMutationFunction_Hook = FindCallInFunction("FixSkillLeveling", SkillMutationFunction_Hook_ID, visitor);
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

                // The SE branch resumes right after the patched call; the AE branch
                // either returns to the caller or jumps past the vanilla loop.
                Code gen{ SkillMutationFunction_Hook + 0x5,
                    isAE ? SkillMutationFunction_Hook + 0x7B : SkillMutationFunction_Hook + 0x5,
                    visitor.address(), isAE };
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
