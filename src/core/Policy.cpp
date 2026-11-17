#include "core/Policy.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <unordered_map>

namespace SOS
{
	namespace
	{
		constexpr std::array<PolicyDef, 12> kPolicies{ {
			{ "XXXX", 100, true },
			{ "XXXE", 101, true },
			{ "XXXO", 2, false },
			{ "XXOX", 102, true },
			{ "XXOE", 103, true },
			{ "XXOO", 1, false },
			{ "XEXX", 104, true },
			{ "XEXE", 105, true },
			{ "XEXO", 3, false },
			{ "XEOX", 106, true },
			{ "XEOE", 107, true },
			{ "XEOO", 108, true },
		} };
	}

	namespace
	{
		struct PolicyAlias
		{
			const char* code;
			const char* aliases[6];
		};

		// Everything a policy can be called by: the code itself, the legacy
		// translation key, the English label of the menu and the Chinese label.
		// The Chinese entries exist twice on purpose: 1.4.2 renamed "套装" to
		// "幻化" in the visible text, but files written by 1.4.1 and older still
		// contain the old wording.
		constexpr PolicyAlias kPolicyAliases[]{
			{ "XXXX", { "$SkyOutSys_Desc_EasyPolicyName_XXXX", "Never show anything", "从不显示任何东西", nullptr, nullptr, nullptr } },
			{ "XXXE", { "$SkyOutSys_Desc_EasyPolicyName_XXXE", "If outfit and equipped, show equipped", "幻化和装备都有时显示装备", "套装和装备都有时显示装备", nullptr, nullptr } },
			{ "XXXO", { "$SkyOutSys_Desc_EasyPolicyName_XXXO", "Require equipped (show the outfit only when the slot is worn)", "需要已装备（槽位有装备时才显示幻化）", "需要已装备（槽位有装备时才显示套装）", "需要已装备", nullptr } },
			{ "XXOX", { "$SkyOutSys_Desc_EasyPolicyName_XXOX", "If only outfit, show outfit", "只有幻化时显示幻化", "只有套装时显示套装", nullptr, nullptr } },
			{ "XXOE", { "$SkyOutSys_Desc_EasyPolicyName_XXOE", "If only outfit show outfit; if both show equipped", "只有幻化显示幻化；都有则显示装备", "只有套装显示套装；都有则显示装备", nullptr, nullptr } },
			{ "XXOO", { "$SkyOutSys_Desc_EasyPolicyName_XXOO", "Always use outfit", "始终使用幻化", "始终使用套装", nullptr, nullptr } },
			{ "XEXX", { "$SkyOutSys_Desc_EasyPolicyName_XEXX", "If only equipped, show equipped", "只有装备时显示装备", nullptr, nullptr, nullptr } },
			{ "XEXE", { "$SkyOutSys_Desc_EasyPolicyName_XEXE", "If equipped, show equipped", "有装备就显示装备", nullptr, nullptr, nullptr } },
			{ "XEXO", { "$SkyOutSys_Desc_EasyPolicyName_XEXO", "Passthrough", "穿透", nullptr, nullptr, nullptr } },
			{ "XEOX", { "$SkyOutSys_Desc_EasyPolicyName_XEOX", "If only equipped show equipped; if only outfit show outfit", "只有装备显示装备；只有幻化显示幻化", "只有装备显示装备；只有套装显示套装", nullptr, nullptr } },
			{ "XEOE", { "$SkyOutSys_Desc_EasyPolicyName_XEOE", "Both, but show equipped when both exist", "两者都显示，但都有时显示装备", nullptr, nullptr, nullptr } },
			{ "XEOO", { "$SkyOutSys_Desc_EasyPolicyName_XEOO", "Both, and show the outfit when both exist", "两者都显示，但都有时显示幻化", "两者都显示，但都有时显示套装", nullptr, nullptr } },
		};

		std::string Trim(std::string_view a_text)
		{
			std::size_t begin = 0;
			std::size_t end = a_text.size();
			const auto isSpace = [](unsigned char a_char) { return std::isspace(a_char) != 0; };
			while (begin < end && isSpace(static_cast<unsigned char>(a_text[begin]))) {
				++begin;
			}
			while (end > begin && isSpace(static_cast<unsigned char>(a_text[end - 1]))) {
				--end;
			}
			return std::string(a_text.substr(begin, end - begin));
		}

		std::string ToUpperAscii(std::string a_text)
		{
			std::transform(a_text.begin(), a_text.end(), a_text.begin(), [](unsigned char a_char) {
				return static_cast<char>(std::toupper(a_char));
			});
			return a_text;
		}

		bool StartsWithInsensitive(const std::string& a_text, const char* a_prefix)
		{
			const std::string prefix(a_prefix);
			return a_text.size() >= prefix.size() && a_text.compare(0, prefix.size(), prefix) == 0;
		}
	}

	std::string NormalizePolicyCode(const std::string& a_name)
	{
		const std::string text = Trim(a_name);
		if (text.empty()) {
			return {};
		}

		// Legacy numeric ids ("0".."11" and the custom range).
		if (std::all_of(text.begin(), text.end(), [](unsigned char a_char) { return std::isdigit(a_char) != 0; })) {
			if (const auto code = PolicyFromInt(static_cast<std::uint32_t>(std::strtoul(text.c_str(), nullptr, 10)))) {
				return *code;
			}
			return {};
		}

		const std::string upper = ToUpperAscii(text);
		for (const auto& entry : kPolicyAliases) {
			if (upper == entry.code) {
				return entry.code;
			}
			for (const char* alias : entry.aliases) {
				if (!alias) {
					continue;
				}
				if (text == alias || upper == ToUpperAscii(alias)) {
					return entry.code;
				}
			}
		}

		// Script defined policies keep their name.
		if (StartsWithInsensitive(upper, "CUSTOMPOLICY")) {
			return text;
		}
		return {};
	}

	const PolicyDef* FindPolicy(const std::string& a_code)
	{
		for (const auto& policy : kPolicies) {
			if (a_code == policy.code) {
				return std::addressof(policy);
			}
		}
		// Saves and hand written JSON files occasionally carry a localized
		// policy *label* instead of the four character code (1.4.1 stored the
		// label when the data came from a translated source). Accept those too
		// instead of silently showing nothing for the slot.
		const auto normalized = NormalizePolicyCode(a_code);
		if (!normalized.empty() && normalized != a_code) {
			for (const auto& policy : kPolicies) {
				if (normalized == policy.code) {
					return std::addressof(policy);
				}
			}
		}
		return nullptr;
	}

	std::optional<std::string> PolicyFromInt(std::uint32_t a_value)
	{
		if (a_value < kPolicies.size()) {
			return std::string(kPolicies[a_value].code);
		}
		if (a_value >= 128) {
			char buffer[32]{};
			std::snprintf(buffer, sizeof(buffer), "CustomPolicy%03u", a_value - 127);
			return std::string(buffer);
		}
		return std::nullopt;
	}

	std::optional<std::uint32_t> PolicyToInt(const std::string& a_code)
	{
		for (std::size_t i = 0; i < kPolicies.size(); ++i) {
			if (a_code == kPolicies[i].code) {
				return static_cast<std::uint32_t>(i);
			}
		}
		// Script policies carry the id in their name: CustomPolicy001 is 128.
		const std::string upper = ToUpperAscii(a_code);
		constexpr const char kPrefix[] = "CUSTOMPOLICY";
		constexpr std::size_t kPrefixLength = sizeof(kPrefix) - 1;
		if (upper.size() > kPrefixLength && StartsWithInsensitive(upper, kPrefix)) {
			const std::string tail = upper.substr(kPrefixLength);
			const bool        digits = std::all_of(tail.begin(), tail.end(), [](unsigned char a_char) { return std::isdigit(a_char) != 0; });
			if (digits) {
				const auto index = static_cast<std::uint32_t>(std::strtoul(tail.c_str(), nullptr, 10));
				if (index >= 1) {
					return 127u + index;
				}
			}
		}
		return std::nullopt;
	}

	const std::vector<const PolicyDef*>& AllPolicies()
	{
		static const std::vector<const PolicyDef*> s_policies = [] {
			std::vector<const PolicyDef*> result;
			result.reserve(kPolicies.size());
			for (const auto& policy : kPolicies) {
				result.push_back(std::addressof(policy));
			}
			std::sort(result.begin(), result.end(), [](const PolicyDef* a_lhs, const PolicyDef* a_rhs) {
				return a_lhs->sort_order < a_rhs->sort_order;
			});
			return result;
		}();
		return s_policies;
	}

	const RE::TESObjectARMO* EvaluatePolicy(
		const PolicyDef& a_policy,
		const RE::TESObjectARMO* a_equipped,
		const RE::TESObjectARMO* a_outfit)
	{
		const bool hasEquipped = a_equipped != nullptr;
		const bool hasOutfit = a_outfit != nullptr;
		const std::size_t index = hasEquipped ? (hasOutfit ? 3 : 1) : (hasOutfit ? 2 : 0);
		const char selection = a_policy.code[index];
		switch (selection) {
		case 'E':
			return a_equipped;
		case 'O':
			return a_outfit;
		default:
			return nullptr;
		}
	}

	std::string TranslationKey(const std::string& a_code)
	{
		if (FindPolicy(a_code)) {
			return "$SkyOutSys_Desc_EasyPolicyName_" + a_code;
		}
		return "$SkyOutSys_Desc_EasyPolicyName_UNKNOWN";
	}
}// namespace SOS
