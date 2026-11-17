#include "hooking/HookSupport.h"

#include <algorithm>
#include <format>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "REL/Module.h"
#include "REL/Offset2ID.h"

namespace Hooking
{
	namespace
	{
		const std::unordered_set<std::uint64_t>& KnownIds()
		{
			// Offset2ID is the only REL interface that exposes the whole mapping table
			// without failing the process on a miss.
			static const std::unordered_set<std::uint64_t> s_ids = [] {
				std::unordered_set<std::uint64_t> ids;
				REL::Offset2ID offset2id;
				ids.reserve(offset2id.size());
				for (const auto& entry : offset2id) {
					ids.insert(entry.id);
				}
				return ids;
			}();
			return s_ids;
		}

		REL::Segment TextSegment()
		{
			return REL::Module::get().segment(REL::Segment::textx);
		}

		bool InText(std::uintptr_t a_address, std::size_t a_size = 1)
		{
			const auto text = TextSegment();
			const auto begin = text.address();
			const auto end = begin + text.size();
			return a_address >= begin && a_address + a_size <= end;
		}

		// Target of the branch located at a_site, or 0 when the bytes there are not one
		// of the short-range branches the engine emits for calls and thunks.
		std::uintptr_t BranchTarget(std::uintptr_t a_site)
		{
			if (!InText(a_site, 5)) return 0;
			const auto* bytes = reinterpret_cast<const std::uint8_t*>(a_site);
			if (bytes[0] == 0xE8 || bytes[0] == 0xE9) {
				const auto rel = *reinterpret_cast<const std::int32_t*>(a_site + 1);
				return a_site + 5 + static_cast<std::uintptr_t>(static_cast<std::ptrdiff_t>(rel));
			}
			if (bytes[0] == 0xEB) {
				const auto rel = *reinterpret_cast<const std::int8_t*>(a_site + 1);
				return a_site + 2 + static_cast<std::uintptr_t>(static_cast<std::ptrdiff_t>(rel));
			}
			return 0;
		}

		// True when the branch at a_site reaches a_expected, directly or through the
		// one-instruction thunk the linker sometimes emits.
		bool Reaches(std::uintptr_t a_site, std::uintptr_t a_expected)
		{
			const auto target = BranchTarget(a_site);
			if (target == 0) return false;
			if (target == a_expected) return true;
			return InText(target, 5) && BranchTarget(target) == a_expected;
		}

		// Every 5-byte call inside [a_begin, a_end) whose target is a_expected. Reading
		// starts are not decoded instructions: a call byte that happens to sit inside
		// another instruction is only reported when its rel32 lands exactly on the
		// expected function, which a coincidence does not produce.
		std::vector<std::uintptr_t> CallsTo(std::uintptr_t a_begin, std::uintptr_t a_end, std::uintptr_t a_expected)
		{
			std::vector<std::uintptr_t> sites;
			const auto text = TextSegment();
			const auto textBegin = text.address();
			const auto textEnd = text.address() + text.size();
			if (a_begin < textBegin + 5) a_begin = textBegin + 5;
			if (a_end > textEnd) a_end = textEnd;
			for (auto address = a_begin; address + 5 <= a_end; ++address) {
				if (*reinterpret_cast<const std::uint8_t*>(address) != 0xE8) continue;
				const auto rel = *reinterpret_cast<const std::int32_t*>(address + 1);
				const auto target = static_cast<std::uintptr_t>(static_cast<std::ptrdiff_t>(address) + 5 + rel);
				if (target == a_expected) sites.push_back(address);
			}
			return sites;
		}

		// Same walk as CallsTo, but a branch that reaches the expected function through
		// the one-instruction thunk the linker emits counts as a hit too.
		std::vector<std::uintptr_t> CallsReaching(std::uintptr_t a_begin, std::uintptr_t a_end, std::uintptr_t a_expected)
		{
			std::vector<std::uintptr_t> sites;
			const auto text = TextSegment();
			const auto textBegin = text.address();
			const auto textEnd = text.address() + text.size();
			if (a_begin < textBegin + 5) a_begin = textBegin + 5;
			if (a_end > textEnd) a_end = textEnd;
			for (auto address = a_begin; address + 5 <= a_end; ++address) {
				if (*reinterpret_cast<const std::uint8_t*>(address) != 0xE8) continue;
				if (!Reaches(address, a_expected)) continue;
				sites.push_back(address);
			}
			return sites;
		}

		// Identity of the running executable: size plus file time distinguishes a retail
		// build from a repacked or patched one, which is what a failing hook comparison
		// usually means.
		std::string ModuleFileStamp()
		{
			wchar_t path[MAX_PATH * 4]{};
			const auto length = ::GetModuleFileNameW(nullptr, path, static_cast<DWORD>(_countof(path)));
			if (length == 0) return std::string{ "path unknown" };

			WIN32_FILE_ATTRIBUTE_DATA attributes{};
			if (!::GetFileAttributesExW(path, GetFileExInfoStandard, &attributes)) return std::string{ "size unknown" };
			const auto size = (static_cast<std::uint64_t>(attributes.nFileSizeHigh) << 32) | attributes.nFileSizeLow;

			SYSTEMTIME utc{};
			SYSTEMTIME local{};
			if (!::FileTimeToSystemTime(&attributes.ftLastWriteTime, &utc) || !::SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local)) {
				return std::format("{} bytes", size);
			}
			return std::format("{} bytes, written {:04}-{:02}-{:02} {:02}:{:02}:{:02}",
				size, local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute, local.wSecond);
		}
	}

	bool IdExists(std::uint64_t a_id)
	{
		return a_id != 0 && KnownIds().contains(a_id);
	}

	bool IdExists(const REL::RelocationID& a_id)
	{
		return IdExists(a_id.id());
	}

	bool RequireIds(const char* a_hook, std::initializer_list<REL::RelocationID> a_ids)
	{
		bool allPresent = true;
		for (const auto& id : a_ids) {
			if (!IdExists(id)) {
				LOG(err, "{}: address library id {} is missing for runtime {}; this hook stays disabled.", a_hook, id.id(), REL::Module::get().version().string());
				allPresent = false;
			}
		}
		return allPresent;
	}

	std::uintptr_t FindCallInFunction(const char* a_hook, const REL::RelocationID& a_host, const REL::RelocationID& a_target)
	{
		if (!IdExists(a_host) || !IdExists(a_target)) {
			LOG(err, "{}: address library id {} or {} is missing for runtime {}; this hook stays disabled.",
				a_hook, a_host.id(), a_target.id(), REL::Module::get().version().string());
			return 0;
		}

		const auto base = REL::Module::get().base();
		const auto entry = a_host.address();
		const auto target = a_target.address();
		const auto text = TextSegment();
		const auto textEnd = text.address() + text.size();
		if (!InText(entry, 1)) {
			LOG(err, "{}: the entry rva 0x{:X} is outside .text; the hook stays disabled so the game keeps running.", a_hook, entry - base);
			return 0;
		}

		auto end = entry + static_cast<std::uintptr_t>(kBodyScanLimit);
		if (end > textEnd) end = textEnd;

		const auto sites = CallsReaching(entry, end, target);
		if (sites.empty()) {
			// Nothing in the body calls the function this hook belongs to. Report enough
			// to identify the build: where the function is, how far the scan looked, and
			// where the whole executable calls that function at all.
			LOG(err, "{}: the function at rva 0x{:X} does not call 0x{:X} within 0x{:X} bytes; the hook stays disabled so the game keeps running. Report this line together with your game version.",
				a_hook, entry - base, target, kBodyScanLimit);
			LogModuleIdentity(a_hook);
			const auto all = CallsReaching(text.address(), textEnd, target);
			LOG(err, "{}: .text holds {} call(s) to 0x{:X}.", a_hook, all.size(), target);
			for (std::size_t i = 0; i < all.size() && i < 5; ++i) {
				LOG(err, "{}: call at rva 0x{:X} ({} bytes from the function entry)", a_hook, all[i] - base,
					static_cast<std::ptrdiff_t>(all[i]) - static_cast<std::ptrdiff_t>(entry));
			}
			return 0;
		}

		for (const auto site : sites) {
			LOG(debug, "{}: the call to 0x{:X} sits at rva 0x{:X} ({} byte(s) into the function at rva 0x{:X}).",
				a_hook, target, site - base,
				static_cast<std::ptrdiff_t>(site) - static_cast<std::ptrdiff_t>(entry), entry - base);
		}
		if (sites.size() > 1) {
			LOG(warn, "{}: the function at rva 0x{:X} calls 0x{:X} {} times; hooking the first one at rva 0x{:X}. If the illusion skips a path in this game build, report this line.",
				a_hook, entry - base, target, sites.size(), sites.front() - base);
		}
		return sites.front();
	}

	std::uintptr_t FindCallInFunctionOrOffset(const char* a_hook, const REL::RelocationID& a_host, const REL::RelocationID& a_target, std::ptrdiff_t a_legacyOffset)
	{
		if (!IdExists(a_host) || !IdExists(a_target)) {
			LOG(err, "{}: address library id {} or {} is missing for runtime {}; this hook stays disabled.",
				a_hook, a_host.id(), a_target.id(), REL::Module::get().version().string());
			return 0;
		}

		const auto base = REL::Module::get().base();
		const auto entry = a_host.address();
		const auto target = a_target.address();
		if (!InText(entry, 1)) {
			LOG(err, "{}: the entry rva 0x{:X} is outside .text; the hook stays disabled so the game keeps running.", a_hook, entry - base);
			return 0;
		}

		const auto text = TextSegment();
		auto end = entry + static_cast<std::uintptr_t>(kBodyScanLimit);
		const auto textEnd = text.address() + text.size();
		if (end > textEnd) end = textEnd;

		const auto sites = CallsReaching(entry, end, target);
		if (!sites.empty()) {
			for (const auto site : sites) {
				LOG(debug, "{}: the call to 0x{:X} sits at rva 0x{:X} ({} byte(s) into the function at rva 0x{:X}).",
					a_hook, target, site - base,
					static_cast<std::ptrdiff_t>(site) - static_cast<std::ptrdiff_t>(entry), entry - base);
			}
			if (sites.size() > 1) {
				LOG(warn, "{}: the function at rva 0x{:X} calls 0x{:X} {} times; hooking the first one at rva 0x{:X}.",
					a_hook, entry - base, target, sites.size(), sites.front() - base);
			}
			return sites.front();
		}

		// No call to the recorded callee in the function body. The instruction the hook
		// has always patched may still be there: this build reaches the skinning pass
		// through a different call, and patching that call is exactly what made the
		// illusion appear in every release up to 1.5.19. Checking the byte keeps the
		// write from landing in the middle of unrelated data.
		const auto legacy = entry + static_cast<std::uintptr_t>(a_legacyOffset);
		if (InText(legacy, 5) && *reinterpret_cast<const std::uint8_t*>(legacy) == 0xE8) {
			LOG(warn, "{}: the function at rva 0x{:X} has no call to 0x{:X} within 0x{:X} bytes, but the site this hook has used since 1.0.0 - rva 0x{:X} (+0x{:X} into the function) - is still a 5-byte call; the hook is installed there.",
				a_hook, entry - base, target, kBodyScanLimit, legacy - base, a_legacyOffset);
			return legacy;
		}

		LOG(err, "{}: the function at rva 0x{:X} does not call 0x{:X} within 0x{:X} bytes and the legacy site rva 0x{:X} is not a 5-byte call either; the hook stays disabled so the game keeps running. Report this line together with your game version.",
			a_hook, entry - base, target, kBodyScanLimit, legacy - base);
		LogModuleIdentity(a_hook);
		const auto all = CallsReaching(text.address(), textEnd, target);
		LOG(err, "{}: .text holds {} call(s) to 0x{:X}.", a_hook, all.size(), target);
		for (std::size_t i = 0; i < all.size() && i < 5; ++i) {
			LOG(err, "{}: call at rva 0x{:X} ({} bytes from the function entry)", a_hook, all[i] - base,
				static_cast<std::ptrdiff_t>(all[i]) - static_cast<std::ptrdiff_t>(entry));
		}
		return 0;
	}

	std::uintptr_t FindExpectedCallAtOffset(const char* a_hook, const REL::RelocationID& a_host, const REL::RelocationID& a_target, std::size_t a_offset)
	{
		if (!RequireIds(a_hook, { a_host, a_target })) return 0;

		const auto entry = a_host.address();
		const auto expected = a_target.address();
		const auto text = TextSegment();
		const auto begin = text.address();
		const auto size = text.size();
		// Subtraction-based bounds checks reject invalid/overflowing ranges before
		// pointer arithmetic or byte reads, including an offset past the end of .text.
		const bool validEntry = SiteContract::ContainsRange(begin, size, entry, 1);
		const bool validOffset = validEntry && a_offset <= size - (entry - begin);
		const auto nominal = validOffset ? entry + a_offset : entry;
		const bool validRange = validOffset && SiteContract::ContainsRange(begin, size, nominal, 5);
		const auto opcode = validRange ? *reinterpret_cast<const std::uint8_t*>(nominal) : 0;
		const auto target = validRange && opcode == 0xE8 ? BranchTarget(nominal) : 0;
		const bool reachesExpected = validRange && opcode == 0xE8 &&
			SiteContract::ContainsRange(begin, size, expected, 1) &&
			SiteContract::ContainsRange(begin, size, target, 5) && Reaches(nominal, expected);
		if (SiteContract::IsExpectedCallSite(nominal, nominal, validRange, opcode, reachesExpected)) {
			LogSiteBytes(a_hook, nominal);
			return nominal;
		}

		LOG(err, "{}: required host +0x{:X} call-site contract failed; hook stays disabled. Same-callee candidates do not verify the register contract and will NOT be substituted.", a_hook, a_offset);
		LogModuleIdentity(a_hook);
		if (validOffset) {
			LogSiteBytes(a_hook, nominal);
			LogCodeWindow(a_hook, nominal, 0x20, 0x20);
			if (target) LogExternalCallTarget(a_hook, target);
		} else {
			LOG(err, "{}: host/offset is outside .text; no nominal bytes can be read safely.", a_hook);
		}
		if (validEntry) {
			const auto scanSize = std::min(static_cast<std::size_t>(kBodyScanLimit), size - (entry - begin));
			const auto candidates = CallsReaching(entry, entry + scanSize, expected);
			LOG(err, "{}: {} same-callee scan candidate(s), DIAGNOSTIC ONLY (byte scan, not instruction decoding).", a_hook, candidates.size());
			for (const auto candidate : candidates) {
				LOG(err, "{}: diagnostic candidate host +0x{:X}; NOT a replacement site.", a_hook, candidate - entry);
			}
		}
		return 0;
	}

	void LogSiteBytes(const char* a_hook, std::uintptr_t a_site)
	{
		if (!InText(a_site, 5)) {
			LOG(err, "{}: the site 0x{:X} is outside .text.", a_hook, a_site - REL::Module::get().base());
			return;
		}
		const auto* bytes = reinterpret_cast<const std::uint8_t*>(a_site);
		LOG(debug, "{}: site rva 0x{:X} -> {:02X} {:02X} {:02X} {:02X} {:02X}",
			a_hook, a_site - REL::Module::get().base(),
			bytes[0], bytes[1], bytes[2], bytes[3], bytes[4]);
	}

	void LogCodeWindow(const char* a_hook, std::uintptr_t a_center, std::ptrdiff_t a_before, std::ptrdiff_t a_after)
	{
		const auto start = a_center - static_cast<std::uintptr_t>(a_before);
		const auto size = static_cast<std::size_t>(a_before + a_after);
		if (!InText(start, size)) {
			LOG(debug, "{}: the window around 0x{:X} leaves .text; not dumped.", a_hook, a_center - REL::Module::get().base());
			return;
		}
		const auto base = REL::Module::get().base();
		for (std::size_t row = 0; row < size; row += 16) {
			std::string hex;
			for (std::size_t i = 0; i < 16 && row + i < size; ++i) {
				hex += std::format("{:02X} ", *reinterpret_cast<const std::uint8_t*>(start + row + i));
			}
			LOG(debug, "{}: rva 0x{:X}: {}", a_hook, start + row - base, hex);
		}
	}

	void LogExternalCallTarget(const char* a_hook, std::uintptr_t a_target)
	{
		MEMORY_BASIC_INFORMATION info{};
		if (!a_target || !::VirtualQuery(reinterpret_cast<const void*>(a_target), &info, sizeof(info))) {
			LOG(warn, "{}: call destination 0x{:X} has no queryable memory region; diagnostic only.", a_hook, a_target);
			return;
		}
		HMODULE owner = nullptr;
		char path[1024]{};
		const bool hasOwner = ::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCSTR>(a_target), &owner) != 0;
		if (hasOwner) ::GetModuleFileNameA(owner, path, sizeof(path) - 1);
		const auto ownerBase = reinterpret_cast<std::uintptr_t>(owner);
		LOG(warn, "{}: call destination 0x{:X}: allocation 0x{:X}, state 0x{:X}, type 0x{:X}, protection 0x{:X}, owner {}, owner offset 0x{:X}; DIAGNOSTIC ONLY, not a validated call contract.",
			a_hook, a_target, reinterpret_cast<std::uintptr_t>(info.AllocationBase), info.State, info.Type, info.Protect,
			hasOwner ? path : "<no loaded module>", hasOwner ? a_target - ownerBase : 0);
		if (info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return;
		const auto region = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
		if (a_target < region || a_target - region >= info.RegionSize) return;
		const auto count = std::min<std::size_t>(32, info.RegionSize - (a_target - region));
		std::uint8_t bytes[32]{};
		SIZE_T read = 0;
		// ReadProcessMemory fails safely if a third party unmaps the region after VirtualQuery.
		const bool copied = ::ReadProcessMemory(::GetCurrentProcess(), reinterpret_cast<const void*>(a_target), bytes, count, &read) != 0;
		if (!copied || !read || read > sizeof(bytes)) {
			LOG(warn, "{}: call destination bytes unavailable (Win32 error {}); no direct dereference attempted.", a_hook, ::GetLastError());
			return;
		}
		std::string hex;
		for (SIZE_T i = 0; i < read; ++i) hex += std::format("{:02X} ", bytes[i]);
		LOG(warn, "{}: call destination bytes at 0x{:X}: {} (no execution, no writes).", a_hook, a_target, hex);
		// Decode only the observed inline absolute-jump slot; never execute it.
		if (read >= 14 && bytes[0] == 0xFF && bytes[1] == 0x25 &&
			bytes[2] == 0 && bytes[3] == 0 && bytes[4] == 0 && bytes[5] == 0) {
			std::uintptr_t destination = 0;
			for (std::size_t i = 0; i < sizeof(destination); ++i)
				destination |= static_cast<std::uintptr_t>(bytes[6 + i]) << (8 * i);
			HMODULE destinationOwner = nullptr;
			char destinationPath[1024]{};
			const bool owned = ::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCSTR>(destination), &destinationOwner) != 0;
			if (owned) ::GetModuleFileNameA(destinationOwner, destinationPath, sizeof(destinationPath) - 1);
			LOG(warn, "{}: inline FF25 destination 0x{:X}, owner {}, owner offset 0x{:X}; DIAGNOSTIC ONLY, chain not approved.",
				a_hook, destination, owned ? destinationPath : "<no loaded module>",
				owned ? destination - reinterpret_cast<std::uintptr_t>(destinationOwner) : 0);
			std::uint8_t entry[32]{};
			SIZE_T entryRead = 0;
			if (::ReadProcessMemory(::GetCurrentProcess(), reinterpret_cast<const void*>(destination), entry, sizeof(entry), &entryRead) && entryRead <= sizeof(entry)) {
				std::string entryHex;
				for (SIZE_T i = 0; i < entryRead; ++i) entryHex += std::format("{:02X} ", entry[i]);
				LOG(warn, "{}: inline FF25 entry bytes: {} (bounded read, no execution, no writes).", a_hook, entryHex);
			}
		}
	}

	void LogModuleIdentity(const char* a_hook)
	{
		const auto& module = REL::Module::get();
		const auto text = TextSegment();
		LOG(info, "{}: SkyrimSE.exe {}, {}, base 0x{:X}, .text rva 0x{:X} + 0x{:X}.",
			a_hook, module.version().string(), ModuleFileStamp(), module.base(), text.address() - module.base(), text.size());
	}

	std::uintptr_t ResolveSite(const char* a_hook, const REL::RelocationID& a_id, std::ptrdiff_t a_offset, std::uintptr_t a_expectedTarget, std::ptrdiff_t a_scanWindow)
	{
		if (!IdExists(a_id)) {
			LOG(err, "{}: address library id {} is missing for runtime {}; this hook stays disabled.", a_hook, a_id.id(), REL::Module::get().version().string());
			return 0;
		}

		const auto base = REL::Module::get().base();
		const auto site = a_id.address() + static_cast<std::uintptr_t>(a_offset);
		if (!InText(site, 5)) {
			LOG(err, "{}: the site at +0x{:X} is outside .text; the hook stays disabled so the game keeps running.", a_hook, a_offset);
			LogModuleIdentity(a_hook);
			return 0;
		}

		const auto opcode = *reinterpret_cast<const std::uint8_t*>(site);
		const auto siteRva = site - base;
		if (opcode == 0x90) {
			LOG(err, "{}: the site at +0x{:X} is already a NOP; the hook stays disabled so the game keeps running. Report this line together with your game version.", a_hook, a_offset);
			LogSiteBytes(a_hook, site);
			return 0;
		}

		if (a_expectedTarget == 0) {
			if (opcode != 0xE8 && opcode != 0xE9) {
				LOG(err, "{}: unexpected opcode 0x{:02X} at +0x{:X} (expected a 5-byte call/jmp); the hook stays disabled so the game keeps running. Report this line together with your game version.", a_hook, opcode, a_offset);
				LogSiteBytes(a_hook, site);
				return 0;
			}
			LogSiteBytes(a_hook, site);
			return site;
		}

		if (opcode == 0xE8 && BranchTarget(site) == a_expectedTarget) {
			LOG(debug, "{}: verified the call target 0x{:X} at +0x{:X}.", a_hook, a_expectedTarget, a_offset);
			LogSiteBytes(a_hook, site);
			return site;
		}

		if (opcode == 0xE9) {
			LOG(warn, "{}: the site at +0x{:X} was already redirected to 0x{:X} (expected 0x{:X}); patching on top of that branch.", a_hook, a_offset, BranchTarget(site), a_expectedTarget);
			LogSiteBytes(a_hook, site);
			return site;
		}

		if (opcode == 0xE8 && Reaches(site, a_expectedTarget)) {
			LOG(debug, "{}: the call at +0x{:X} reaches the expected 0x{:X} through the thunk 0x{:X}.", a_hook, a_offset, a_expectedTarget, BranchTarget(site));
			LogSiteBytes(a_hook, site);
			return site;
		}

		LOG(err, "{}: the call at +0x{:X} targets 0x{:X}, but 0x{:X} was expected; the hook stays disabled so the game keeps running. Report this line together with your game version.", a_hook, a_offset, BranchTarget(site), a_expectedTarget);
		LogSiteBytes(a_hook, site);
		LogCodeWindow(a_hook, site, 0x10, 0x10);
		LogModuleIdentity(a_hook);

		const auto text = TextSegment();
		const auto all = CallsTo(text.address(), text.address() + text.size(), a_expectedTarget);

		if (a_scanWindow > 0) {
			const auto begin = site - static_cast<std::uintptr_t>(a_scanWindow);
			const auto end = site + static_cast<std::uintptr_t>(a_scanWindow) + 5;
			const auto sites = CallsTo(begin, end, a_expectedTarget);
			if (sites.size() == 1) {
				const auto found = sites.front();
				LOG(warn, "{}: the call to 0x{:X} sits at rva 0x{:X} instead of 0x{:X} (moved by {} bytes); hooking the relocated site.",
					a_hook, a_expectedTarget, found - base, siteRva,
					static_cast<std::ptrdiff_t>(found) - static_cast<std::ptrdiff_t>(site));
				LogSiteBytes(a_hook, found);
				return found;
			}
			if (sites.size() > 1) {
				LOG(err, "{}: {} calls to 0x{:X} sit inside the +/-0x{:X} window; refusing to guess which one belongs to this hook.",
					a_hook, sites.size(), a_expectedTarget, a_scanWindow);
				for (const auto candidate : sites) {
					LOG(err, "{}: candidate at rva 0x{:X} ({} bytes from the nominal site)",
						a_hook, candidate - base, static_cast<std::ptrdiff_t>(candidate) - static_cast<std::ptrdiff_t>(site));
				}
			} else {
				LOG(err, "{}: no call to 0x{:X} inside the +/-0x{:X} window either.", a_hook, a_expectedTarget, a_scanWindow);
				// When the whole executable calls the target exactly once, that call is the
				// only place this hook can belong to, so it is safe to take it even when it
				// sits further away than the window allows.
				if (all.size() == 1) {
					const auto found = all.front();
					LOG(warn, "{}: .text holds exactly one call to 0x{:X}, at rva 0x{:X} ({} bytes from the nominal site); hooking that one.",
						a_hook, a_expectedTarget, found - base,
						static_cast<std::ptrdiff_t>(found) - static_cast<std::ptrdiff_t>(site));
					LogSiteBytes(a_hook, found);
					return found;
				}
			}
		}

		// Whole-.text survey: shows whether the expected function is called at all and
		// where the nearest call sits, which is what identifies the right offset next.
		std::vector<std::pair<std::ptrdiff_t, std::uintptr_t>> ranked;
		ranked.reserve(all.size());
		for (const auto candidate : all) {
			const auto delta = static_cast<std::ptrdiff_t>(candidate) - static_cast<std::ptrdiff_t>(site);
			ranked.emplace_back(delta < 0 ? -delta : delta, candidate);
		}
		std::sort(ranked.begin(), ranked.end());
		LOG(err, "{}: .text holds {} call(s) to 0x{:X}.", a_hook, all.size(), a_expectedTarget);
		for (std::size_t i = 0; i < ranked.size() && i < 5; ++i) {
			LOG(err, "{}: nearest call at rva 0x{:X} ({} bytes from the nominal site)",
				a_hook, ranked[i].second - base, static_cast<std::ptrdiff_t>(ranked[i].second) - static_cast<std::ptrdiff_t>(site));
		}
		return 0;
	}
	}

