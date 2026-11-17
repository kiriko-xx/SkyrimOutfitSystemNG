#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace RE
{
	class TESObjectARMO;
}

namespace SOS
{
	// One of the twelve built-in visibility policies. The four characters
	// describe what to display for the four combinations of
	// (equipped?, in outfit?) for a single body slot:
	//   0: neither  -> chars[0]
	//   1: equipped only -> chars[1]
	//   2: outfit only   -> chars[2]
	//   3: both          -> chars[3]
	// 'E' shows the equipped item, 'O' shows the outfit item, 'X' shows nothing.
	struct PolicyDef
	{
		const char* code;
		std::uint8_t sort_order;
		bool advanced;
	};

	const PolicyDef* FindPolicy(const std::string& a_code);

	// Turns anything a policy may be called by (four character code, the legacy
	// "$SkyOutSys_Desc_EasyPolicyName_XXXX" key, the English label or one of the
	// Chinese labels) into the canonical code. Returns an empty string when the
	// name is not a built in policy and not a script policy.
	std::string NormalizePolicyCode(const std::string& a_name);

	// The numeric ids the original protobuf saves used for a policy:
	// 0..11 follow the declaration order of the built-in table, anything from
	// 128 up is a user defined policy ("CustomPolicy001" ...). Returns an empty
	// optional for the gap in between, mirroring Policy::from_int of the
	// original Rust implementation.
	std::optional<std::string> PolicyFromInt(std::uint32_t a_value);

	// The inverse of PolicyFromInt: the numeric id the legacy protobuf enum
	// (and the OutfitSystemData.json files written by 0.6.x) uses for a policy
	// code. Returns an empty optional for anything that is not a built in or
	// script policy name.
	std::optional<std::uint32_t> PolicyToInt(const std::string& a_code);
	const std::vector<const PolicyDef*>& AllPolicies();

	// Returns the armor that should be displayed for one slot, or nullptr.
	const RE::TESObjectARMO* EvaluatePolicy(
		const PolicyDef& a_policy,
		const RE::TESObjectARMO* a_equipped,
		const RE::TESObjectARMO* a_outfit);

	// "$SkyOutSys_Desc_EasyPolicyName_XXOO" style translation key.
	std::string TranslationKey(const std::string& a_code);
}// namespace SOS
