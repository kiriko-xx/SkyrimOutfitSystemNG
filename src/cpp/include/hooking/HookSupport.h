#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include "REL/Relocation.h"

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
