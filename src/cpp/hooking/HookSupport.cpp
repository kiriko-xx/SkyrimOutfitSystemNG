#include "hooking/HookSupport.h"

#include <algorithm>
#include <format>
#include <limits>
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

		// Committed and readable across [a_address, a_address + a_size). The pages under
		// these addresses can belong to another module, so a plain dereference is not an
		// option: reading an uncommitted page kills the process.
		bool Readable(std::uintptr_t a_address, std::size_t a_size)
		{
			auto at = a_address;
			auto left = a_size;
			while (left > 0) {
				MEMORY_BASIC_INFORMATION info{};
				if (::VirtualQuery(reinterpret_cast<LPCVOID>(at), &info, sizeof(info)) == 0) return false;
				if (info.State != MEM_COMMIT) return false;
				if ((info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) return false;
				const auto regionEnd = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
				if (regionEnd <= at) return false;
				const auto step = static_cast<std::size_t>(regionEnd - at);
				const auto used = step < left ? step : left;
				at += used;
				left -= used;
			}
			return true;
		}

		// True when the page holding a_address holds code. A detour written by another
		// plugin may live in a region that plugin allocated next to the game executable, so
		// the check is on the protection the page ended up with, not on its module.
		bool Executable(std::uintptr_t a_address)
		{
			MEMORY_BASIC_INFORMATION info{};
			if (::VirtualQuery(reinterpret_cast<LPCVOID>(a_address), &info, sizeof(info)) == 0) return false;
			if (info.State != MEM_COMMIT) return false;
			if (info.Type == MEM_IMAGE) return true;
			switch (info.Protect & 0xFF) {
			case PAGE_EXECUTE:
			case PAGE_EXECUTE_READ:
			case PAGE_EXECUTE_READWRITE:
			case PAGE_EXECUTE_WRITECOPY:
				return true;
			default:
				return false;
			}
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
		// True when a rip-relative load of a_target sits in [a_begin, a_end). The visitor a
		// pass hands to the worn call is built on the stack and takes its vtable from one
		// address, so the lea right before the call names the pass.
		bool LoadsAddress(std::uintptr_t a_begin, std::uintptr_t a_end, std::uintptr_t a_target)
		{
			if (!a_target || !InText(a_begin, 1)) return false;
			for (auto at = a_begin; at + 7 <= a_end; ++at) {
				const auto* bytes = reinterpret_cast<const std::uint8_t*>(at);
				if (bytes[0] != 0x48 && bytes[0] != 0x4C) continue;
				if (bytes[1] != 0x8D) continue;
				if ((bytes[2] & 0xC7) != 0x05) continue;
				const auto rel = *reinterpret_cast<const std::int32_t*>(at + 3);
				const auto address = at + 7 + static_cast<std::uintptr_t>(static_cast<std::ptrdiff_t>(rel));
				if (address == a_target) return true;
			}
			return false;
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

	std::uintptr_t FindVerifiedCall(const char* a_hook, const REL::RelocationID& a_host, const REL::RelocationID& a_target, const SiteShape& a_shape)
	{
		if (!IdExists(a_host) || !IdExists(a_target)) {
			LOG(err, "{}: address library id {} or {} is missing for runtime {}; this hook stays disabled.",
				a_hook, a_host.id(), a_target.id(), REL::Module::get().version().string());
			return 0;
		}

		// A malformed fingerprint would silently turn every candidate into "unverified",
		// so refuse to install instead of running with a shape that cannot match.
		for (std::size_t i = 0; i < a_shape.chunks.size(); ++i) {
			if (SitePattern::Length(a_shape.chunks[i].bytes) == 0) {
				LOG(critical, "{}: byte fingerprint {} of the '{}' site shape is malformed; the hook stays disabled.", a_hook, i, a_shape.name);
				return 0;
			}
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

		std::vector<std::uintptr_t> verified;
		for (const auto site : sites) {
			LOG(debug, "{}: the call to 0x{:X} sits at rva 0x{:X} ({} byte(s) into the function at rva 0x{:X}).",
				a_hook, target, site - base,
				static_cast<std::ptrdiff_t>(site) - static_cast<std::ptrdiff_t>(entry), entry - base);
			const auto* bytes = reinterpret_cast<const std::uint8_t*>(site);
			if (SitePattern::MatchesAll(a_shape.chunks.data(), a_shape.chunks.size(), bytes)) {
				verified.push_back(site);
			}
		}

		if (verified.size() == 1) {
			LOG(debug, "{}: rva 0x{:X} is the site of this hook (matched '{}').", a_hook, verified.front() - base, a_shape.name);
			return verified.front();
		}

		if (verified.size() > 1) {
			LOG(err, "{}: {} calls to 0x{:X} inside the function at rva 0x{:X} match '{}'; the hook stays disabled rather than patching a guess. Report this line together with your game version.",
				a_hook, verified.size(), target, entry - base, a_shape.name);
			return 0;
		}

		if (sites.size() == 1) {
			// Fingerprints were written against the builds that have been looked at. A game
			// build nobody has looked at yet still leaves a single call to the callee in the
			// function, which is the evidence this hook was written around.
			LOG(warn, "{}: the only call to 0x{:X} in the function at rva 0x{:X} does not match '{}'; hooking it anyway because it is the only one there. Report this line together with your game version.",
				a_hook, target, entry - base, a_shape.name);
			return sites.front();
		}

		LOG(err, "{}: {} calls to 0x{:X} inside the function at rva 0x{:X} and none matches '{}'; the hook stays disabled so the game keeps running. Report this line together with your game version.",
			a_hook, sites.size(), target, entry - base, a_shape.name);
		LogModuleIdentity(a_hook);
		return 0;
	}

	bool DeriveLoopSkip(const char* a_hook, std::uintptr_t a_entry, std::uintptr_t a_site, std::uintptr_t* a_skip)
	{
		if (!a_skip) return false;
		const auto base = REL::Module::get().base();
		const auto begin = a_site + 5;
		auto end = a_site + static_cast<std::uintptr_t>(kLoopScanLimit);
		const auto text = TextSegment();
		const auto textEnd = text.address() + text.size();
		if (end > textEnd) end = textEnd;

		std::vector<std::pair<std::uintptr_t, std::size_t>> backEdges;
		for (auto address = begin; address + 2 <= end; ++address) {
			const auto opcode = *reinterpret_cast<const std::uint8_t*>(address);
			std::size_t length = 0;
			std::ptrdiff_t rel = 0;
			if (opcode >= 0x70 && opcode <= 0x7F) {
				length = 2;
				rel = *reinterpret_cast<const std::int8_t*>(address + 1);
			} else if (opcode == 0x0F && address + 6 <= end) {
				const auto second = *reinterpret_cast<const std::uint8_t*>(address + 1);
				if (second >= 0x80 && second <= 0x8F) {
					length = 6;
					rel = *reinterpret_cast<const std::int32_t*>(address + 2);
				}
			}
			if (!length) continue;
			const auto target = static_cast<std::uintptr_t>(static_cast<std::ptrdiff_t>(address) + static_cast<std::ptrdiff_t>(length) + rel);
			// A loop back edge jumps backwards, but its target can sit *after* the call
			// site: the call is one of the statements inside the loop body, and the body
			// starts further on. Requiring the target to be before the call would miss
			// exactly the loop this is meant to find.
			if (target >= a_entry && target < address) {
				backEdges.emplace_back(address, length);
			}
		}

		if (backEdges.size() != 1) {
			LOG(err, "{}: the call site at rva 0x{:X} is followed by {} branches that jump back into the loop; the hook stays disabled because leaving the loop would end up mid-instruction. Report this line together with your game version.",
				a_hook, a_site - base, backEdges.size());
			return false;
		}

		*a_skip = backEdges.front().first + backEdges.front().second;
		LOG(debug, "{}: the loop around rva 0x{:X} ends at rva 0x{:X}, behind the branch at rva 0x{:X}.",
			a_hook, a_site - base, *a_skip - base, backEdges.front().first - base);
		return true;
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

	void LogModuleIdentity(const char* a_hook)
	{
		const auto& module = REL::Module::get();
		const auto text = TextSegment();
		LOG(info, "{}: SkyrimSE.exe {}, {}, base 0x{:X}, .text rva 0x{:X} + 0x{:X}.",
			a_hook, module.version().string(), ModuleFileStamp(), module.base(), text.address() - module.base(), text.size());
	}

	namespace
	{
		// ModRM byte of "mov [rsp+disp8], r64" (mod = 01, rm = 100 = the SIB byte that
		// follows). 0x44 / 0x4C / 0x54 / 0x5C / 0x64 / 0x6C / 0x74 / 0x7C: the registers a
		// compiler spills into the home space at the start of a function.
		bool IsStackStoreModrm(std::uint8_t a_modrm)
		{
			switch (a_modrm) {
			case 0x44:
			case 0x4C:
			case 0x54:
			case 0x5C:
			case 0x64:
			case 0x6C:
			case 0x74:
			case 0x7C:
				return true;
			default:
				return false;
			}
		}
	}

	std::size_t CopyableEntryLength(std::uintptr_t a_entry, std::size_t a_needed)
	{
		const auto* bytes = reinterpret_cast<const std::uint8_t*>(a_entry);
		std::size_t used = 0;
		while (used < a_needed) {
			const auto left = a_needed - used;
			const auto* at = bytes + used;
			std::size_t length = 0;
			if (left >= 5 && at[0] == 0x48 && at[1] == 0x89 && at[3] == 0x24 && IsStackStoreModrm(at[2])) {
				length = 5;// mov [rsp+disp8], rbx / rbp / rsi / rdi / rax / rcx / rdx / rsp
			} else if (left >= 5 && at[0] == 0x4C && at[1] == 0x89 && at[3] == 0x24 && IsStackStoreModrm(at[2])) {
				length = 5;// mov [rsp+disp8], r8 .. r15
			} else if (left >= 3 && at[0] == 0x48 && at[1] == 0x8B && (at[2] == 0xC4 || at[2] == 0xEC)) {
				length = 3;// mov rax, rsp / mov rbp, rsp
			} else if (left >= 3 && at[0] == 0x4C && at[1] == 0x8B && at[2] == 0xDC) {
				length = 3;// mov r11, rsp
			} else if (left >= 5 && at[0] == 0x48 && at[1] == 0x8D && (at[2] == 0x6C || at[2] == 0x64) && at[3] == 0x24) {
				length = 5;// lea rbp / rsp, [rsp+disp8]
			} else if (left >= 4 && at[0] == 0x48 && at[1] == 0x83 && (at[2] == 0xEC || at[2] == 0xE4)) {
				length = 4;// sub rsp, imm8 / and rsp, imm8
			} else if (left >= 7 && at[0] == 0x48 && at[1] == 0x81 && at[2] == 0xEC) {
				length = 7;// sub rsp, imm32
			} else if (left >= 2 && at[0] == 0x41 && at[1] >= 0x50 && at[1] <= 0x57) {
				length = 2;// push r8 .. r15
			} else if (left >= 2 && at[0] == 0x40 && at[1] >= 0x50 && at[1] <= 0x57) {
				length = 2;// push rbx / rbp / rsi / rdi behind a redundant REX prefix
			} else if (at[0] >= 0x50 && at[0] <= 0x57) {
				length = 1;// push rax .. rdi
			}
			if (length == 0 || length > left) {
				return 0;
			}
			used += length;
		}
		return used;
	}

	std::string ModuleOwner(std::uintptr_t a_address)
	{
		::HMODULE module = nullptr;
		if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(a_address), &module) ||
			!module) {
			return std::string{ "no module" };
		}
		wchar_t path[MAX_PATH * 4]{};
		if (::GetModuleFileNameW(module, path, static_cast<DWORD>(_countof(path))) == 0) {
			return std::string{ "nameless module" };
		}
		std::wstring_view full{ path };
		const auto cut = full.find_last_of(L"\\/");
		const auto name = cut == std::wstring_view::npos ? full : full.substr(cut + 1);
		std::string narrow;
		narrow.reserve(name.size());
		for (const auto ch : name) {
			narrow.push_back(ch < 0x80 ? static_cast<char>(ch) : '?');
		}
		return std::format("{} + 0x{:X}", narrow, a_address - reinterpret_cast<std::uintptr_t>(module));
	}

	bool BranchInRange(std::uintptr_t a_site, std::uintptr_t a_target)
	{
		const auto displacement = static_cast<std::ptrdiff_t>(a_target) - static_cast<std::ptrdiff_t>(a_site + 5);
		return displacement >= (std::numeric_limits<std::int32_t>::min)() && displacement <= (std::numeric_limits<std::int32_t>::max)();
	}

	std::uintptr_t FollowEntryJumps(const char* a_hook, std::uintptr_t a_entry, std::uintptr_t* a_firstJump)
	{
		constexpr int kMaxHops = 8;
		if (a_firstJump) *a_firstJump = 0;

		auto site = a_entry;
		std::uintptr_t seen[kMaxHops]{};
		int hops = 0;
		while (hops < kMaxHops) {
			if (!Readable(site, 8)) {
				LOG(err, "{}: the bytes at {} cannot be read; the hook stays disabled so the game keeps running.", a_hook, ModuleOwner(site));
				return 0;
			}
			const auto* bytes = reinterpret_cast<const std::uint8_t*>(site);
			const char* shape = nullptr;
			std::uintptr_t target = 0;
			if (bytes[0] == 0xE9) {
				shape = "jmp rel32";
				const auto rel = *reinterpret_cast<const std::int32_t*>(site + 1);
				target = site + 5 + static_cast<std::uintptr_t>(static_cast<std::ptrdiff_t>(rel));
			} else if (bytes[0] == 0xEB) {
				shape = "jmp rel8";
				const auto rel = *reinterpret_cast<const std::int8_t*>(site + 1);
				target = site + 2 + static_cast<std::uintptr_t>(static_cast<std::ptrdiff_t>(rel));
			} else if (bytes[0] == 0xFF && bytes[1] == 0x25) {
				shape = "jmp [rip+disp32]";
				const auto rel = *reinterpret_cast<const std::int32_t*>(site + 2);
				const auto slot = site + 6 + static_cast<std::uintptr_t>(static_cast<std::ptrdiff_t>(rel));
				if (!Readable(slot, sizeof(std::uintptr_t))) {
					LOG(err, "{}: the slot of the {} at {} cannot be read; the hook stays disabled so the game keeps running.", a_hook, shape, ModuleOwner(site));
					return 0;
				}
				target = *reinterpret_cast<const std::uintptr_t*>(slot);
			} else if (bytes[0] == 0x48 && bytes[1] == 0xFF && bytes[2] == 0x25) {
				shape = "jmp [rip+disp32]";
				const auto rel = *reinterpret_cast<const std::int32_t*>(site + 3);
				const auto slot = site + 7 + static_cast<std::uintptr_t>(static_cast<std::ptrdiff_t>(rel));
				if (!Readable(slot, sizeof(std::uintptr_t))) {
					LOG(err, "{}: the slot of the {} at {} cannot be read; the hook stays disabled so the game keeps running.", a_hook, shape, ModuleOwner(site));
					return 0;
				}
				target = *reinterpret_cast<const std::uintptr_t*>(slot);
			} else {
				break;// real code: this is where the hook belongs
			}

			if (target == 0) {
				LOG(err, "{}: the {} at {} has no target; the hook stays disabled so the game keeps running.", a_hook, shape, ModuleOwner(site));
				return 0;
			}
			if (hops == 0 && a_firstJump) *a_firstJump = target;
			for (int i = 0; i < hops; ++i) {
				if (seen[i] == target) {
					LOG(err, "{}: the jumps starting at {} loop back to {}; the hook stays disabled so the game keeps running.", a_hook, ModuleOwner(a_entry), ModuleOwner(target));
					return 0;
				}
			}
			LOG(warn, "{}: {} starts with a {}, which another plugin wrote when it hooked this function; the hook goes on {} instead, so both keep working.", a_hook, ModuleOwner(site), shape, ModuleOwner(target));
			seen[hops++] = target;
			site = target;
		}

		if (hops == kMaxHops) {
			LOG(err, "{}: more than {} jumps in a row start at {}; the hook stays disabled so the game keeps running.", a_hook, kMaxHops, ModuleOwner(a_entry));
			return 0;
		}
		if (!Executable(site)) {
			LOG(err, "{}: the jump chain ends at {}, which holds no code; the hook stays disabled so the game keeps running.", a_hook, ModuleOwner(site));
			return 0;
		}
		return site;
	}

	std::uintptr_t FindWornPassCall(const char* a_hook, const REL::RelocationID& a_host, const REL::RelocationID& a_target, const REL::RelocationID& a_anchorVTable, std::ptrdiff_t a_preferredOffset)
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
		const auto textEnd = text.address() + text.size();
		auto end = entry + static_cast<std::uintptr_t>(kBodyScanLimit);
		if (end > textEnd) end = textEnd;

		const auto sites = CallsReaching(entry, end, target);
		if (sites.empty()) {
			LOG(err, "{}: the function at rva 0x{:X} does not call 0x{:X} within 0x{:X} bytes; the hook stays disabled so the game keeps running and the display set is attached from the deferred pass. Report this line together with your game version.",
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
			LOG(debug, "{}: candidate call to 0x{:X} at rva 0x{:X} (+0x{:X} into the function at rva 0x{:X}).",
				a_hook, target, site - base, static_cast<std::ptrdiff_t>(site) - static_cast<std::ptrdiff_t>(entry), entry - base);
		}

		// The visitor the call is handed is what tells the passes apart.
		const bool anchorKnown = IdExists(a_anchorVTable);
		const auto anchor = anchorKnown ? a_anchorVTable.address() : 0;
		std::vector<std::uintptr_t> anchored;
		if (anchor) {
			const auto lookback = static_cast<std::uintptr_t>(kAnchorLookback);
			for (const auto site : sites) {
				const auto low = site - lookback < entry ? entry : site - lookback;
				if (LoadsAddress(low, site, anchor)) anchored.push_back(site);
			}
		}
		if (anchored.size() == 1) {
			const auto site = anchored.front();
			LOG(info, "{}: the call at rva 0x{:X} (+0x{:X} into the function) is preceded by a load of the visitor 0x{:X} - the pass that fills the biped on this game build, which is where the hook goes.",
				a_hook, site - base, static_cast<std::ptrdiff_t>(site) - static_cast<std::ptrdiff_t>(entry), anchor);
			return site;
		}
		if (anchored.size() > 1) {
			LOG(warn, "{}: {} of the {} call(s) to 0x{:X} are preceded by a load of the visitor 0x{:X}; the recorded offset decides between them.",
				a_hook, anchored.size(), sites.size(), target, anchor);
		} else if (anchorKnown) {
			LOG(warn, "{}: none of the {} call(s) to 0x{:X} is preceded within 0x{:X} bytes by a load of the visitor 0x{:X}.",
				a_hook, sites.size(), target, kAnchorLookback, anchor);
		} else {
			LOG(warn, "{}: the visitor id {} is missing for runtime {}, so the passes cannot be told apart by the visitor they are handed.",
				a_hook, a_anchorVTable.id(), REL::Module::get().version().string());
		}

		const auto preferred = entry + static_cast<std::uintptr_t>(a_preferredOffset);
		if (a_preferredOffset != 0 && InText(preferred, 5) && *reinterpret_cast<const std::uint8_t*>(preferred) == 0xE8 && BranchTarget(preferred) == target) {
			LOG(warn, "{}: the hook goes to the site this plugin has patched since 1.0.0, rva 0x{:X} (+0x{:X} into the function), which still calls 0x{:X}. If the illusion does not appear on this game build, report this line together with your game version.",
				a_hook, preferred - base, a_preferredOffset, target);
			return preferred;
		}

		if (sites.size() > 1) {
			LOG(err, "{}: the function at rva 0x{:X} calls 0x{:X} {} times and none of them could be identified as the pass that fills the biped, so the hook stays disabled and the display set is attached from the deferred pass. Report this line together with your game version.",
				a_hook, entry - base, target, sites.size());
			LogModuleIdentity(a_hook);
			return 0;
		}

		LOG(debug, "{}: the function at rva 0x{:X} calls 0x{:X} once, at rva 0x{:X}; the hook goes there.", a_hook, entry - base, target, sites.front() - base);
		return sites.front();
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

