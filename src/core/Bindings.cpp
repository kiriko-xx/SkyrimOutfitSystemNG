#include "bindings.h"

#include "core/Policy.h"
#include "core/Settings.h"

#include <cctype>
#include <string_view>

namespace
{
	int CompareNatural(std::string_view a_lhs, std::string_view a_rhs)
	{
		std::size_t i = 0;
		std::size_t j = 0;
		while (i < a_lhs.size() && j < a_rhs.size()) {
			const auto ca = static_cast<unsigned char>(a_lhs[i]);
			const auto cb = static_cast<unsigned char>(a_rhs[j]);
			if (std::isdigit(ca) && std::isdigit(cb)) {
				// Compare whole runs of digits numerically, so that "Outfit 2"
				// sorts before "Outfit 10".
				std::size_t iEnd = i;
				std::size_t jEnd = j;
				while (iEnd < a_lhs.size() && std::isdigit(static_cast<unsigned char>(a_lhs[iEnd]))) {
					++iEnd;
				}
				while (jEnd < a_rhs.size() && std::isdigit(static_cast<unsigned char>(a_rhs[jEnd]))) {
					++jEnd;
				}
				std::string_view lhsDigits = a_lhs.substr(i, iEnd - i);
				std::string_view rhsDigits = a_rhs.substr(j, jEnd - j);
				while (lhsDigits.size() > 1 && lhsDigits.front() == '0') {
					lhsDigits.remove_prefix(1);
				}
				while (rhsDigits.size() > 1 && rhsDigits.front() == '0') {
					rhsDigits.remove_prefix(1);
				}
				if (lhsDigits.size() != rhsDigits.size()) {
					return lhsDigits.size() < rhsDigits.size() ? -1 : 1;
				}
				if (const int cmp = lhsDigits.compare(rhsDigits); cmp != 0) {
					return cmp < 0 ? -1 : 1;
				}
				i = iEnd;
				j = jEnd;
				continue;
			}
			const int la = std::tolower(ca);
			const int lb = std::tolower(cb);
			if (la != lb) {
				return la < lb ? -1 : 1;
			}
			++i;
			++j;
		}
		if (i < a_lhs.size()) {
			return 1;
		}
		if (j < a_rhs.size()) {
			return -1;
		}
		return 0;
	}
}

bool is_form_id_permitted(std::uint32_t a_formID)
{
	return a_formID < 0xFF000000;
}

std::vector<MetadataC> list_available_policies_c(bool a_allowAdvanced)
{
	std::vector<MetadataC> result;
	for (const auto* policy : SOS::AllPolicies()) {
		if (!policy) {
			continue;
		}
		if (policy->advanced && !a_allowAdvanced) {
			continue;
		}
		MetadataC entry;
		entry.name = rust::String(policy->code);
		entry.sort_order = policy->sort_order;
		entry.advanced = policy->advanced;
		result.push_back(std::move(entry));
	}
	return result;
}

rust::String translation_key_c(const rust::String& a_code)
{
	return rust::String(SOS::TranslationKey(a_code));
}

std::int8_t nat_ord_case_insensitive_c(const rust::String& a_lhs, const rust::String& a_rhs)
{
	return static_cast<std::int8_t>(CompareNatural(a_lhs, a_rhs));
}

bool settings_extra_logging_enabled()
{
	return SOS::Settings::Get().extraLogging;
}
