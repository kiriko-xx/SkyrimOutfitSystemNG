// Site fingerprints: the byte checks a hook runs to prove that the call it found is the
// one it was written against. The byte strings below are the ones recorded from
// SkyrimSE 1.5.97 and 1.6.1170, so a change to a pattern in the plugin shows up here as
// a failure instead of as a hook that stops matching in game.
//
// Compiled and run by tests/site_pattern.ps1 against src/cpp/include.

#include "hooking/SitePattern.h"

#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <string_view>
#include <vector>

namespace
{
	int g_failures = 0;

	void Check(bool a_condition, const char* a_what)
	{
		if (!a_condition) {
			std::printf("FAIL %s\n", a_what);
			++g_failures;
		}
	}

	using Hooking::SitePattern::Chunk;

	std::vector<std::uint8_t> Bytes(std::string_view a_hex)
	{
		std::vector<std::uint8_t> bytes;
		int high = -1;
		for (const char c : a_hex) {
			if (c == ' ') continue;
			const int digit = Hooking::SitePattern::HexDigit(c);
			if (digit < 0) continue;
			if (high < 0) {
				high = digit;
			} else {
				bytes.push_back(static_cast<std::uint8_t>((high << 4) | digit));
				high = -1;
			}
		}
		return bytes;
	}

	// Lays the recorded bytes out the way the game has them - the code leading into the
	// call, the call itself, then the code after it - and reports whether the shape
	// accepts that site. The five call bytes are placeholders: no fingerprint describes
	// them, because their rel32 differs on every build.
	bool Hits(std::string_view a_pre, std::string_view a_post, std::initializer_list<Chunk> a_chunks)
	{
		const auto pre = Bytes(a_pre);
		std::vector<std::uint8_t> buffer = pre;
		for (int i = 0; i < 5; ++i) buffer.push_back(0xE8);
		const auto post = Bytes(a_post);
		buffer.insert(buffer.end(), post.begin(), post.end());
		const auto* site = buffer.data() + pre.size();
		const auto chunks = std::vector<Chunk>(a_chunks);
		return Hooking::SitePattern::MatchesAll(chunks.data(), chunks.size(), site);
	}

	void Expect(std::string_view a_pre, std::string_view a_post, std::initializer_list<Chunk> a_chunks,
		bool a_expected, const char* a_what)
	{
		// Every recorded byte string has to be exactly as long as its chunk claims,
		// otherwise a typo in a pattern would silently move the comparison.
		const auto pre = Bytes(a_pre);
		const auto chunks = std::vector<Chunk>(a_chunks);
		for (const auto& chunk : chunks) {
			if (chunk.offset >= 0) continue;
			if (static_cast<std::ptrdiff_t>(pre.size()) >= -chunk.offset) continue;
			std::printf("FAIL %s (the call is preceded by %zu bytes, the chunk starts %td before it)\n",
				a_what, pre.size(), chunk.offset);
			++g_failures;
			return;
		}
		Check(Hits(a_pre, a_post, a_chunks) == a_expected, a_what);
	}
}

int main()
{
	using Hooking::SitePattern::Length;
	using Hooking::SitePattern::Matches;

	// ---- hex pattern parsing ---------------------------------------------
	Check(Length("48 8B") == 2, "two bytes");
	Check(Length("40 B? 01") == 3, "a wildcard is one byte");
	Check(Length(" 48  8B ") == 2, "extra spaces are ignored");
	Check(Length("48 8B 0") == 0, "half a byte is rejected");
	Check(Length("48 8G") == 0, "non hex is rejected");
	Check(Length("") == 0, "an empty pattern is rejected");

	const std::vector<std::uint8_t> shim{ 0x48, 0x8B, 0xC8, 0x4C, 0x89, 0x74, 0x24, 0x70 };
	Check(Matches("48 8B C8", shim.data()), "exact bytes match");
	Check(!Matches("48 8B CD", shim.data()), "a different register does not");
	Check(Matches("4C 89 ?? 24 70", shim.data() + 3), "a whole byte wildcard matches anything");
	Check(Matches("48 8? C8", shim.data()), "a nibble wildcard matches");
	Check(!Matches("48 89", shim.data()), "the operand byte is not accepted");
	Check(!Matches("48 8B C8", nullptr), "a null site never matches");

	// ---- vanilla player skinning (DontVanillaSkinPlayer) ------------------
	const std::initializer_list<Chunk> kVanillaShape{
		{ -18, "44 8B 49 38 41 83 E1 01 48 8B 91 58 01 00 00 48 8B CD" },
		{ 5, "40 B? 01 40 0F B6 C?" },
	};
	const std::string_view kVanillaPre("44 8B 49 38 41 83 E1 01 48 8B 91 58 01 00 00 48 8B CD");
	Expect(kVanillaPre, "40 B7 01 40 0F B6 C7", kVanillaShape, true, "1.5.97 vanilla armour site matches");
	Expect(kVanillaPre, "40 B6 01 40 0F B6 C6", kVanillaShape, true, "1.6.1170 vanilla armour site matches");
	Expect("C3 44 8B 49 38 48 8B 91 58 01 00 00 41 83 E1 01 49 8B CA", "B0 01 48 83 C4 28 C3",
		kVanillaShape, false, "the call in the next function is not mistaken for it");

	// ---- worn flags shim (ShimWornFlags) ----------------------------------
	const std::initializer_list<Chunk> kWornMaskShape{ { -8, "48 8B C8 4C 89 ?? 24 70" } };
	Expect("48 8B C8 4C 89 64 24 70", "8B 8D 2C 01 00 00", kWornMaskShape, true, "1.5.97 worn mask site matches");
	Expect("48 8B C8 4C 89 74 24 70", "41 8B 8F 2C 01 00 00", kWornMaskShape, true, "1.6.1170 worn mask site matches");

	// ---- equip conflict loop (FixEquipConflictCheck) ----------------------
	const std::initializer_list<Chunk> kConflictShape{
		{ -8, "4C 8B F0 8B D3 48 8B CD" },
		{ 5, "84 C0 0F 84" },
	};
	Expect("4C 8B F0 8B D3 48 8B CD", "84 C0 0F 84 B1 01 00 00", kConflictShape, true, "1.5.97 conflict loop site matches");
	Expect("4C 8B F0 8B D3 48 8B CD", "84 C0 0F 84 B1 01 00 00", kConflictShape, true, "1.6.1170 conflict loop site matches");
	Expect("01 00 00 8B D3 48 8B C8", "84 C0 0F 84 32 01 00 00", kConflictShape, false,
		"the inner scan of the same form is not mistaken for the slot loop");
	Expect("10 75 1B 8B D7 48 8B CD", "84 C0 74 0D", kConflictShape, false,
		"the call through edi is not mistaken for the slot loop");

	// ---- skill mutation (FixSkillLeveling) --------------------------------
	const std::initializer_list<Chunk> kSkillShapeSE{
		{ -10, "48 8D 54 24 20 48 89 44 24 28" },
		{ 5, "8B 7C 24 34 8B 74 24 30 03 FE" },
	};
	const std::initializer_list<Chunk> kSkillShapeAE{
		{ -6, "4C 8B E8 48 8B 0E" },
		{ 5, "48 8B D8 4C 8B 36 49 83 C6 10 45 8D 67 2A" },
	};
	Expect("48 8D 54 24 20 48 89 44 24 28", "8B 7C 24 34 8B 74 24 30 03 FE 74 4D E8", kSkillShapeSE, true,
		"1.5.97 skill mutation site matches");
	Expect("4C 8B E8 48 8B 0E", "48 8B D8 4C 8B 36 49 83 C6 10 45 8D 67 2A", kSkillShapeAE, true,
		"1.6.1170 skill mutation site matches");
	Expect("CC CC CC CC 4C 8B E8 48 8B 0E", "48 8B D8 4C 8B 36 49 83 C6 10 45 8D 67 2A", kSkillShapeSE, false,
		"the SE fingerprints do not accept the inlined AE loop");
	Expect("20 48 89 44 24 28", "8B 7C 24 34 8B 74 24 30 03 FE", kSkillShapeAE, false,
		"the AE fingerprints do not accept the SE call");

	// ---- a malformed fingerprint never matches ----------------------------
	const std::initializer_list<Chunk> kBroken{ { -2, "48 8B 0" } };
	const std::uint8_t site[8]{ 0x48, 0x8B, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x00 };
	Check(!Hooking::SitePattern::MatchesAll(kBroken.begin(), kBroken.size(), site + 4),
		"a malformed fingerprint never matches");
	Check(!Hooking::SitePattern::MatchesAll(nullptr, 0, site + 4), "an empty shape never matches");

	if (g_failures != 0) {
		std::printf("SITE_PATTERN_FAILED: %d check(s)\n", g_failures);
		return 1;
	}
	std::printf("SITE_PATTERN_OK\n");
	return 0;
}
