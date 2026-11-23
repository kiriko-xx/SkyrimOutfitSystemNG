#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

// Byte fingerprints for a hook site.
//
// A hook that patches a call inside a game function has to prove that it found the
// right instruction before it writes anything. The address library only knows where
// functions start, so the call itself is found by scanning the body for a branch to
// the callee - and a function may branch to the callee from more than one place, or
// the scan window may reach into the function that follows (the vanilla skinning
// routine has a second call to ApplyArmorAddon in the next function along).
//
// A fingerprint is a few bytes of the instruction stream at a fixed distance from
// the call: the argument setup that leads into it and the code that consumes its
// result. Those bytes follow from the source, not from a recorded offset, so a build
// that recompiles the function, moves the call, or reorders its branches still
// matches while a different call to the same callee does not.
//
// Patterns are written as hex bytes separated by spaces, with ? as a nibble
// wildcard: "40 B? 01" accepts 40 B6 01 and 40 B7 01, which is how the same
// instruction looks on two runtimes that allocate a register differently.
namespace Hooking::SitePattern
{
	// One run of bytes that has to sit at a fixed distance from the call. A negative
	// offset describes the bytes leading into the call; the run has to end exactly at
	// the call instruction.
	struct Chunk
	{
		std::ptrdiff_t offset;
		std::string_view bytes;// hex, space separated, ? = wildcard nibble
	};

	constexpr int HexDigit(char a_char)
	{
		if (a_char >= '0' && a_char <= '9') return a_char - '0';
		if (a_char >= 'a' && a_char <= 'f') return a_char - 'a' + 10;
		if (a_char >= 'A' && a_char <= 'F') return a_char - 'A' + 10;
		return -1;
	}

	// Number of bytes a pattern describes, or 0 when it is malformed (odd nibble,
	// trailing half byte, unknown character).
	inline std::size_t Length(std::string_view a_pattern)
	{
		std::size_t count = 0;
		for (std::size_t i = 0; i < a_pattern.size();) {
			const char hi = a_pattern[i];
			if (hi == ' ') {
				++i;
				continue;
			}
			if (i + 1 >= a_pattern.size()) return 0;
			const char lo = a_pattern[i + 1];
			if (hi != '?' && HexDigit(hi) < 0) return 0;
			if (lo != '?' && HexDigit(lo) < 0) return 0;
			++count;
			i += 2;
		}
		return count;
	}

	// True when a_bytes starts with the bytes a_pattern describes.
	inline bool Matches(std::string_view a_pattern, const std::uint8_t* a_bytes)
	{
		if (!a_bytes) return false;
		std::size_t index = 0;
		for (std::size_t i = 0; i < a_pattern.size();) {
			const char hi = a_pattern[i];
			if (hi == ' ') {
				++i;
				continue;
			}
			if (i + 1 >= a_pattern.size()) return false;
			const char lo = a_pattern[i + 1];
			const auto byte = a_bytes[index++];
			if (hi != '?' && HexDigit(hi) != static_cast<int>((byte >> 4) & 0xF)) return false;
			if (lo != '?' && HexDigit(lo) != static_cast<int>(byte & 0xF)) return false;
			i += 2;
		}
		return index != 0;
	}

	// True when every chunk sits where it claims to, relative to a_site.
	inline bool MatchesAll(const Chunk* a_chunks, std::size_t a_count, const std::uint8_t* a_site)
	{
		if (!a_chunks || !a_count || !a_site) return false;
		for (std::size_t i = 0; i < a_count; ++i) {
			const auto& chunk = a_chunks[i];
			if (Length(chunk.bytes) == 0) return false;
			if (!Matches(chunk.bytes, a_site + chunk.offset)) return false;
		}
		return true;
	}
}
