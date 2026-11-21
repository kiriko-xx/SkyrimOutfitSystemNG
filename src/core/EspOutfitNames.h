#pragma once

#include <Windows.h>
#include <cstdint>
#include <string>
#include <string_view>

#pragma comment(lib, "Normaliz.lib")

namespace SOS::EspOutfits::Names
{
	inline std::u32string ComparisonKey(std::string_view text)
	{
		if (text.empty() || text.size() > 1024) {
			return {};
		}
		const auto size = static_cast<int>(text.size());
		const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, nullptr, 0);
		if (count <= 0) {
			return {};
		}
		std::wstring wide(count, L'\0');
		if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, wide.data(), count) != count) {
			return {};
		}
		const int capacity = NormalizeString(NormalizationKC, wide.data(), count, nullptr, 0);
		if (capacity <= 0) {
			return {};
		}
		std::wstring normalized(capacity, L'\0');
		int length = NormalizeString(NormalizationKC, wide.data(), count, normalized.data(), capacity);
		if (length < 0 && -length > capacity) {
			normalized.resize(-length);
			length = NormalizeString(NormalizationKC, wide.data(), count, normalized.data(), -length);
		}
		if (length <= 0) {
			return {};
		}
		const int lowerSize = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, normalized.data(), length,
			nullptr, 0, nullptr, nullptr, 0);
		if (lowerSize <= 0) {
			return {};
		}
		std::wstring lower(lowerSize, L'\0');
		if (LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, normalized.data(), length,
			lower.data(), lowerSize, nullptr, nullptr, 0) != lowerSize) {
			return {};
		}
		std::u32string key;
		for (std::size_t i = 0; i < lower.size(); ++i) {
			char32_t ch = lower[i];
			if (ch >= 0xD800 && ch <= 0xDBFF && i + 1 < lower.size()) {
				ch = 0x10000 + ((ch - 0xD800) << 10) + (lower[++i] - 0xDC00);
			}
			// Optional joiners, not token boundaries or required separators.
			if (ch == U'_' || ch == U'-' || ch == U' ' || (ch >= 9 && ch <= 13) ||
				ch == 0x85 || ch == 0x1680 || (ch >= 0x2000 && ch <= 0x200A) ||
				ch == 0x2028 || ch == 0x2029 || ch == 0x202F || ch == 0x205F || ch == 0x3000) {
				continue;
			}
			key.push_back(ch);
		}
		return key;
	}

	inline std::string OriginalPrefix(std::string_view text, std::u32string_view prefix)
	{
		if (prefix.empty()) {
			return {};
		}
		for (std::size_t i = 1; i <= text.size(); ++i) {
			if (i < text.size() && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) {
				continue;
			}
			if (ComparisonKey(text.substr(0, i)) == prefix) {
				return std::string(text.substr(0, i));
			}
		}
		return {};  // e.g. prefix ends inside an NFKC expansion
	}

	inline std::string SlotNumbers(std::uint32_t mask)
	{
		std::string numbers;
		for (unsigned i = 0; i < 32; ++i) {
			if ((mask & (1u << i)) != 0) {
				if (!numbers.empty()) {
					numbers += ", ";
				}
				numbers += std::to_string(i + 30);
			}
		}
		return numbers;
	}
}
