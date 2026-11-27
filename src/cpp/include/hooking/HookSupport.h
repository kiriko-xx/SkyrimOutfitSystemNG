#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

#include "REL/Relocation.h"
#include "hooking/SitePattern.h"

namespace Hooking
{
	// True if the address library of the running game build contains this id.
	// REL::ID::address() aborts the process when the id is unknown, so every id is
	// checked through here before it is ever resolved.
	bool IdExists(std::uint64_t a_id);

	// Variant used by the hooks: only the id for the current runtime is examined.
	bool IdExists(const REL::RelocationID& a_id);

	// Checks every id a hook depends on. Logs one line per missing id and returns
	// false when at least one of them is unavailable.
	bool RequireIds(const char* a_hook, std::initializer_list<REL::RelocationID> a_ids);

	// Locates the injected call inside the function the address library entry a_host
	// points at. The database only knows where a function starts, so the body is
	// walked from that entry (at most kBodyScanLimit bytes, never outside .text) and
	// every 5-byte call that reaches a_target - directly or through the
	// one-instruction thunk the linker emits - is collected.
	//
	// This is what keeps the hooks free of per-runtime byte offsets: when a build
	// recompiles a function and its internal call moves, the hook finds it again at
	// the new address instead of disabling itself on a byte comparison. The first
	// candidate is returned and hooked; any further candidates are logged, because a
	// function that legitimately calls the target twice has to be handled by hand
	// while a second candidate in a neighbouring function only means the scan window
	// reached past the end of this one.
	//
	// Returns 0 (and logs why) when no call to a_target sits in the body.
	std::uintptr_t FindCallInFunction(const char* a_hook, const REL::RelocationID& a_host, const REL::RelocationID& a_target);

	// A set of byte fingerprints that identify one call site inside a game function.
	// Every chunk has to match for the site to count as verified.
	struct SiteShape
	{
		const char* name;
		std::vector<SitePattern::Chunk> chunks;
	};

	// Like FindCallInFunction, but the site has to be identified, not guessed.
	//
	// The candidates are the calls in the body of a_host that reach a_target. A
	// candidate counts as verified when every chunk of a_shape matches the
	// instruction stream around it (the argument setup leading into the call and the
	// code that consumes the result). The rules are:
	//   * exactly one verified candidate - the usual case - that one is patched;
	//   * several verified candidates - the fingerprints do not separate them, so the
	//     hook is disabled; patching the wrong one of two indistinguishable calls is
	//     worse than not patching at all;
	//   * no verified candidate but exactly one candidate - a game build that the
	//     fingerprints were not written for. The single call to the callee inside the
	//     function is still the best evidence there is, so it is patched with a
	//     warning that keeps the case visible in the log;
	//   * no verified candidate and several candidates - the hook is disabled.
	//
	// Returns 0 (and logs why) instead of ever patching an unidentified site.
	std::uintptr_t FindVerifiedCall(const char* a_hook, const REL::RelocationID& a_host, const REL::RelocationID& a_target, const SiteShape& a_shape);

	// Distance past a call site that is scanned for the loop it sits in. The skill
	// mutation loop of AE 1.6.1170 closes 0x79 bytes after the call.
	inline constexpr std::ptrdiff_t kLoopScanLimit = 0x140;

	// Finds the instruction the injected code has to jump to in order to leave the loop
	// the call site sits in: the one right after the loop back edge, i.e. after the only
	// branch in [a_site + 5, a_site + kLoopScanLimit) that jumps backwards into the loop.
	//
	// AE inlined the routine this hook used to patch, so the skip target is a compile
	// artifact rather than something the address library knows. Deriving it from the
	// branch structure instead of a recorded "+0x7B" keeps the fix working when a build
	// recompiles the loop, and refusing to guess keeps a disabled hook from jumping into
	// the middle of an instruction.
	bool DeriveLoopSkip(const char* a_hook, std::uintptr_t a_entry, std::uintptr_t a_site, std::uintptr_t* a_skip);

	// Same scan, but for builds where the function body contains no call to a_target at
	// all. That happens when the compiler inlined the callee or when the address
	// library entry resolves to a different function than the one the hook was written
	// against. In that case the site the plugin has patched since 1.0.0 (host entry +
	// a_legacyOffset, a per-hook constant) is used when the bytes there are still a
	// 5-byte call. Returning 0 would disable the hook, and a disabled CustomSkinPlayer
	// hook leaves the real armour hidden with nothing attached, which is worse than a
	// site that does not match the recorded callee.
	std::uintptr_t FindCallInFunctionOrOffset(const char* a_hook, const REL::RelocationID& a_host, const REL::RelocationID& a_target, std::ptrdiff_t a_legacyOffset);

	// Locates the call inside the skinning routine of a hook's host function that hands
	// an item visitor to a_target, and - when that function calls a_target more than once -
	// tells the passes apart by the visitor object itself. AE 1.6.1170 calls the worn
	// visitor twice: once with a checker that only reports what is worn, and once with the
	// visitor that fills the BipedAnim. Patching the first one changes nothing while the
	// hook that hides the real armour still runs, which is exactly the nude actor 1.5.56
	// produced on that runtime.
	//
	// The anchor is the address of the vtable of the visitor the wanted pass loads right
	// before the call; every candidate site is checked for a rip-relative load of it within
	// kAnchorLookback bytes. Nothing here trusts a recorded byte offset: a_preferredOffset
	// is only used when it still holds a call to a_target, and when the candidates stay
	// ambiguous the function returns 0 - a disabled hook leaves the illusion to the deferred
	// pass, while a guessed site hides the real armour for the rest of the session.
	std::uintptr_t FindWornPassCall(const char* a_hook, const REL::RelocationID& a_host, const REL::RelocationID& a_target, const REL::RelocationID& a_anchorVTable, std::ptrdiff_t a_preferredOffset);

	// Length of the entry bytes of a function that can be copied verbatim into a
	// trampoline, or 0 when the prologue cannot be relocated safely.
	//
	// Hooking a callee at its entry instead of a call site inside its caller keeps this
	// plugin off a piece of code other plugins patch as well. The AE 1.6.1170 skinning
	// routine calls the worn visitor twice, and one of those calls no longer reads as a
	// plain call at run time because another plugin claims it; the entry of the callee is
	// reached no matter who rewrote its callers.
	//
	// Only instructions that run the same wherever they are placed are accepted (stores
	// of the non-volatile registers relative to rsp, and pushes of volatile ones), and
	// a_needed must land on an instruction boundary. Anything else returns 0 so the
	// caller does not copy a rip-relative or relative instruction into the trampoline.
	std::size_t CopyableEntryLength(std::uintptr_t a_entry, std::size_t a_needed);

	// Checks that the bytes at a_entry are still the ones this hook was written for.
	//
	// Hooking a function at its entry assumes the address library points at the function
	// itself and that nobody replaced its prologue since. Both assumptions can be wrong:
	// SKSE ships two address libraries for some game versions (1.6.1170 has "-0" and
	// "-0-1", and 98.7% of the shared ids sit at different addresses in them, while
	// CommonLib only ever loads "-0"), and another plugin may have hooked the same function
	// first. Writing five bytes at either place would corrupt unrelated code, so the entry
	// is compared with the prologue of the game function the hook was written for.
	//
	// Returns true when the bytes match. Otherwise it logs what is there instead - a branch
	// means someone hooked the function first, anything else means the address library
	// describes another build - and returns false so the caller leaves the hook disabled.
	bool EntryMatches(const char* a_hook, std::uintptr_t a_entry, const std::uint8_t* a_expected, std::size_t a_size);

	// Identity of the module that owns a_address - the game executable or an SKSE plugin
	// dll - in the form "<file name> + 0x<rva>". A log line that names the owner says
	// whose code sits at an address; a bare pointer does not.
	std::string ModuleOwner(std::uintptr_t a_address);

	// Follows the branch another plugin wrote at a function entry when it hooked that
	// function before this plugin.
	//
	// Hooking a function at its entry assumes the entry still holds the prologue of the
	// game function. When another plugin has already replaced it with a five-byte jump to
	// its own detour, the bytes there are a relative branch: copied into a trampoline they
	// would branch to the wrong place, so the entry hook used to disable itself and the
	// illusion stayed off for the whole session (reported on 1.5.64, SE 1.5.97, with a
	// run time entry of E9 2B AE D7 FF where the shipped executable has 48 89 5C 24 10).
	//
	// Chaining on top of the other plugin keeps both of them working: the branch stays
	// where it is, and this hook goes on the code the branch lands on. That detour runs to
	// completion - it returns to the injected code like a function returns to its caller -
	// and only then is the display set attached, exactly as when the entry belongs to the
	// game function itself.
	//
	// Returns a_entry itself when its first bytes are real code, the end of the jump chain
	// otherwise, and 0 - after logging why - when the chain cannot be followed safely
	// (unreadable memory, a jump that loops, more than eight hops, a target that is not
	// executable code). a_firstJump, when given, receives the target of the branch at
	// a_entry, or 0 when there was none.
	std::uintptr_t FollowEntryJumps(const char* a_hook, std::uintptr_t a_entry, std::uintptr_t* a_firstJump = nullptr);

	// True when a five-byte relative branch at a_site can reach a_target. CommonLib's
	// trampoline terminates the process when the displacement does not fit, which is fine
	// for its own writers - they know both ends - but not for a hook that may land on code
	// another plugin owns: there the branch has to be bridged, or the hook has to stay
	// disabled instead of taking the game down with it.
	bool BranchInRange(std::uintptr_t a_site, std::uintptr_t a_target);

	// Size of the stub AllocateFarStub writes: "jmp qword ptr [rip+0]" plus the address it
	// jumps to.
	inline constexpr std::size_t kFarStubSize = 14;

	// Reserves executable memory within reach of a_site and writes an absolute jump to
	// a_target into it, so the five-byte branch at a_site reaches code that lies farther
	// away than a rel32 can express.
	//
	// A hook site is not always inside the game executable: following the branch another
	// plugin wrote at a function entry lands in that plugin's dll, and Windows maps a dll
	// wherever the address space allows. The trampoline this plugin allocates sits next to
	// the executable, so for such a site the distance is routinely more than the two
	// gigabytes a rel32 covers. 1.5.78 only measured that distance and left the hook
	// disabled, which switched the illusion off for the whole session (SE 1.5.97, the worn
	// visitor chained onto InventoryLagFix.dll + 0x2B70). The stub removes the assumption
	// instead of guarding it: the branch at the site stays short, and the stub does the long
	// jump without touching a register or the stack, so the code it serves keeps its own
	// calling convention.
	//
	// Returns 0 - after logging why - when no page within reach can be reserved; the caller
	// has to leave the hook disabled then.
	std::uintptr_t AllocateFarStub(const char* a_hook, std::uintptr_t a_site, std::uintptr_t a_target);

	// Writes the five-byte branch at a_site that lands on a_target - the one thing
	// CommonLib writes for the hooks of this plugin.
	//
	// CommonLib write_branch keeps its own "jmp qword ptr [rip+0]" slot inside the
	// trampoline next to the game executable and ends the process when a_site is farther
	// from that slot than a rel32 reaches (Trampoline.cpp:215, "displacement is out of
	// range"). The distance from a_site to the injected code is a different question, so
	// 1.5.79 passed its own check, handed the inflated worn visitor site of AE 1.6.1170
	// (InventoryLagFix.dll + 0x2B70, about 14 GB from the executable) to write_branch, and
	// the game refused to start. Here both ends are known and alive - the site holds the
	// bytes about to be replaced, the target was just allocated in reach - so the branch is
	// written into the site directly and no third address enters the calculation.
	//
	// Returns false - after logging why - when the branch cannot be placed; the caller has
	// to leave the hook disabled then.
	bool WriteBranchTo(const char* a_hook, std::uintptr_t a_site, std::uintptr_t a_target);

	// Reads the branch at a_site back and compares where it leads with a_target.
	//
	// WriteBranchTo can copy the bytes and still be undone by the next writer: several
	// plugins hook the same engine functions, and the one that writes last owns the site.
	// Reading the five bytes back is the only way to separate "installed" from "installed and
	// still there". A branch another plugin overwrites afterwards fails silently, which is
	// what a report of "the illusion does nothing" looks like.
	//
	// Returns true when the branch leads to a_target. On a mismatch it logs what the bytes
	// are now and returns false; it never repairs the site, because the plugin that wrote
	// there last did so on purpose.
	bool VerifyBranch(const char* a_hook, std::uintptr_t a_site, std::uintptr_t a_target);

	// Every branch this plugin wrote, so all of them can be read back once the whole load
	// order has had its turn.
	void RegisterSite(const char* a_hook, std::uintptr_t a_site, std::uintptr_t a_target);

	// Reads every registered branch back and reports the ones another plugin has taken over.
	// Meant to run late (kDataLoaded and kPostLoadGame): a plugin that hooks the same
	// function during its own data-loaded pass would otherwise write after the check.
	//
	// Returns the number of branches that are no longer this plugin's.
	std::size_t VerifyInstalledSites();

	// Logs the five-byte jumps inside [a_begin, a_end) that leave the executable.
	//
	// Called when a hook cannot find the call it was written to patch: a call another plugin
	// replaced with a jump to its own detour is not a call any more, and the log has to say
	// that instead of leaving "this game build does not call that function" as the only
	// explanation. Byte scanning without an instruction decoder can hit an 0xE9 that sits
	// inside another instruction, so only jumps landing outside .text - i.e. in a plugin -
	// are reported.
	void LogForeignJumps(const char* a_hook, std::uintptr_t a_begin, std::uintptr_t a_end);

	// How far before a call the visitor it is handed is loaded. Both passes of the AE
	// skinning routine load theirs inside this window.
	inline constexpr std::ptrdiff_t kAnchorLookback = 0x60;
	// How far into a function body the call scan looks. All the hooks patch a call in
	// the first few hundred bytes of their host function (the deepest is 0x373).
	inline constexpr std::ptrdiff_t kBodyScanLimit = 0x800;

	// Resolves a hook site at a fixed offset from an address library entry, checks that
	// the bytes there are still a 5-byte call/jmp and, when a_expectedTarget is given,
	// that the branch goes to that address. Returns 0 (after logging why) when the
	// instruction is not what was expected.
	//
	// Kept for the few sites that are not "call this function": every skinning hook uses
	// FindCallInFunction instead, so no hook depends on a recorded byte offset any more.
	// Passing 0 for a_expectedTarget keeps the byte-only check.
	std::uintptr_t ResolveSite(const char* a_hook, const REL::RelocationID& a_id, std::ptrdiff_t a_offset, std::uintptr_t a_expectedTarget = 0, std::ptrdiff_t a_scanWindow = 0);

	// Dumps the first bytes at a site so a failing install can be diagnosed from the
	// log alone.
	void LogSiteBytes(const char* a_hook, std::uintptr_t a_site);

	// Dumps a hex window around an address together with the identity of the running
	// executable (version, size, file time, .text range). Used when a hook cannot be
	// placed: the pair tells whether the exe itself differs from the retail build.
	void LogCodeWindow(const char* a_hook, std::uintptr_t a_center, std::ptrdiff_t a_before, std::ptrdiff_t a_after);
	void LogModuleIdentity(const char* a_hook);
}
