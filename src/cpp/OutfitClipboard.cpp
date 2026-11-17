#include "OutfitClipboard.h"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cwchar>
#include <exception>
#include <optional>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include "bindings.h"
#include "core/Outfit.h"
#include "core/OutfitService.h"
#include "core/Policy.h"

namespace SOS::OutfitClipboard
{
	namespace
	{
		constexpr const char* kFallbackName = "Outfit";
		constexpr int         kMaxCopySuffix = 100;

		// ---- clipboard -----------------------------------------------------
		bool SetClipboardText(const std::string& a_text)
		{
			if (a_text.empty()) {
				return false;
			}
			const int wideLength = ::MultiByteToWideChar(CP_UTF8, 0, a_text.c_str(), static_cast<int>(a_text.size()), nullptr, 0);
			if (wideLength <= 0) {
				return false;
			}
			std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
			if (::MultiByteToWideChar(CP_UTF8, 0, a_text.c_str(), static_cast<int>(a_text.size()), wide.data(), wideLength) <= 0) {
				return false;
			}
			const std::size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
			HGLOBAL           memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
			if (!memory) {
				return false;
			}
			void* buffer = ::GlobalLock(memory);
			if (!buffer) {
				::GlobalFree(memory);
				return false;
			}
			std::memcpy(buffer, wide.c_str(), bytes);
			::GlobalUnlock(memory);

			if (!::OpenClipboard(nullptr)) {
				::GlobalFree(memory);
				return false;
			}
			::EmptyClipboard();
			if (!::SetClipboardData(CF_UNICODETEXT, memory)) {
				::CloseClipboard();
				::GlobalFree(memory);
				return false;
			}
			// The clipboard owns the block now; do not free it.
			::CloseClipboard();
			return true;
		}

		bool GetClipboardText(std::string& a_text, bool& a_opened)
		{
			a_text.clear();
			a_opened = false;
			if (!::IsClipboardFormatAvailable(CF_UNICODETEXT)) {
				return false;
			}
			if (!::OpenClipboard(nullptr)) {
				return false;
			}
			a_opened = true;
			bool ok = false;
			if (HANDLE data = ::GetClipboardData(CF_UNICODETEXT)) {
				if (const auto* wide = static_cast<const wchar_t*>(::GlobalLock(data))) {
					const std::size_t length = ::wcslen(wide);
					if (length > 0) {
						const int utf8Length = ::WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(length), nullptr, 0, nullptr, nullptr);
						if (utf8Length > 0) {
							a_text.resize(static_cast<std::size_t>(utf8Length));
							if (::WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(length), a_text.data(), utf8Length, nullptr, nullptr) > 0) {
								ok = true;
							} else {
								a_text.clear();
							}
						}
					}
					::GlobalUnlock(data);
				}
			}
			::CloseClipboard();
			return ok;
		}

		// ---- policies ------------------------------------------------------
		std::optional<std::string> ReadPolicyValue(const nlohmann::json& a_value)
		{
			if (a_value.is_number_unsigned()) {
				return PolicyFromInt(a_value.get<std::uint32_t>());
			}
			if (a_value.is_number_integer()) {
				const auto number = a_value.get<std::int64_t>();
				if (number >= 0 && number <= 0xFFFF) {
					return PolicyFromInt(static_cast<std::uint32_t>(number));
				}
				return std::nullopt;
			}
			if (a_value.is_string()) {
				const auto code = NormalizePolicyCode(a_value.get<std::string>());
				if (!code.empty()) {
					return code;
				}
			}
			return std::nullopt;
		}

		std::optional<std::string> ReadPolicy(const nlohmann::json& a_entry, std::initializer_list<const char*> a_keys)
		{
			if (!a_entry.is_object()) {
				return std::nullopt;
			}
			for (const char* key : a_keys) {
				if (const auto it = a_entry.find(key); it != a_entry.end()) {
					if (const auto code = ReadPolicyValue(*it)) {
						return code;
					}
				}
			}
			return std::nullopt;
		}

		// ---- armour pieces -------------------------------------------------
		std::uint32_t ReadUint(const nlohmann::json& a_entry, std::initializer_list<const char*> a_keys)
		{
			if (!a_entry.is_object()) {
				return 0;
			}
			for (const char* key : a_keys) {
				if (const auto it = a_entry.find(key); it != a_entry.end()) {
					if (it->is_number_unsigned()) {
						return it->get<std::uint32_t>();
					}
					if (it->is_number_integer()) {
						const auto number = it->get<std::int64_t>();
						if (number >= 0) {
							return static_cast<std::uint32_t>(number);
						}
					}
				}
			}
			return 0;
		}

		std::string ReadString(const nlohmann::json& a_entry, std::initializer_list<const char*> a_keys)
		{
			if (!a_entry.is_object()) {
				return {};
			}
			for (const char* key : a_keys) {
				if (const auto it = a_entry.find(key); it != a_entry.end() && it->is_string()) {
					return it->get<std::string>();
				}
			}
			return {};
		}

		const RE::TESObjectARMO* ResolveArmor(const nlohmann::json& a_entry)
		{
			if (!a_entry.is_object()) {
				return nullptr;
			}
			const auto modName = ReadString(a_entry, { "modName", "mod_name", "plugin" });
			const auto localId = ReadUint(a_entry, { "localFormId", "local_form_id" });
			if (!modName.empty() && localId != 0) {
				if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
					if (auto* armor = dataHandler->LookupForm<RE::TESObjectARMO>(localId, modName)) {
						return armor;
					}
				}
			}
			const auto rawId = ReadUint(a_entry, { "rawFormId", "raw_form_id", "form" });
			if (rawId != 0) {
				// A raw form id only means something in the load order it was
				// written in; let SKSE remap it before looking it up, otherwise a
				// paste from another save points at an unrelated armour.
				RE::FormID id = static_cast<RE::FormID>(rawId);
				if (auto* serialization = SKSE::GetSerializationInterface()) {
					RE::FormID resolved = 0;
					if (serialization->ResolveFormID(id, resolved) && resolved != 0) {
						id = resolved;
					}
				}
				return RE::TESForm::LookupByID<RE::TESObjectARMO>(id);
			}
			return nullptr;
		}

		// ---- names ---------------------------------------------------------
		std::string TrimName(std::string_view a_name)
		{
			std::size_t begin = 0;
			std::size_t end = a_name.size();
			while (begin < end && std::isspace(static_cast<unsigned char>(a_name[begin]))) {
				++begin;
			}
			while (end > begin && std::isspace(static_cast<unsigned char>(a_name[end - 1]))) {
				--end;
			}
			return std::string(a_name.substr(begin, end - begin));
		}

		// Cuts a UTF-8 string to at most a_maxBytes without splitting a code point.
		std::string TruncateUtf8(const std::string& a_text, std::size_t a_maxBytes)
		{
			if (a_text.size() <= a_maxBytes) {
				return a_text;
			}
			std::size_t length = a_maxBytes;
			while (length > 0 && (static_cast<unsigned char>(a_text[length]) & 0xC0) == 0x80) {
				--length;
			}
			return a_text.substr(0, length);
		}

		// "name", "name_copy", "name_copy2", ... - the first one that is free.
		std::string FreeName(OutfitService& a_service, const std::string& a_base, std::size_t a_maxLength)
		{
			if (!a_service.has_outfit(a_base.c_str())) {
				return a_base;
			}
			for (int copy = 1; copy <= kMaxCopySuffix; ++copy) {
				std::string suffix = copy == 1 ? std::string("_copy") : ("_copy" + std::to_string(copy));
				std::string candidate = TruncateUtf8(a_base, a_maxLength > suffix.size() ? a_maxLength - suffix.size() : 0) + suffix;
				if (!candidate.empty() && !a_service.has_outfit(candidate.c_str())) {
					return candidate;
				}
			}
			return {};
		}
	}

	bool Copy(const std::string& a_outfitName)
	{
		nlohmann::json entry;
		{
			auto service = outfit_service_get_singleton_ptr();
			const auto* outfit = service->inner().get_outfit_ptr(a_outfitName.c_str());
			if (!outfit) {
				return false;
			}
			entry["name"] = std::string(outfit->name_c());
			if (outfit->favorite_c()) {
				entry["isFavorite"] = true;
			}

			auto armors = nlohmann::json::array();
			for (const auto* armor : outfit->armors()) {
				if (!armor) {
					continue;
				}
				nlohmann::json piece;
				piece["rawFormId"] = armor->GetFormID();
				if (const auto localId = armor->GetLocalFormID(); localId != 0) {
					piece["localFormId"] = localId;
				}
				if (const auto* file = armor->GetFile(0); file && file->fileName[0]) {
					piece["modName"] = file->fileName;
				}
				armors.push_back(std::move(piece));
			}
			entry["armors"] = std::move(armors);

			auto slots = nlohmann::json::object();
			for (std::uint32_t slot = 0; slot < kSlotCount; ++slot) {
				const auto& policy = outfit->slot_policy(slot);
				if (!policy || policy->empty()) {
					continue;
				}
				if (const auto number = PolicyToInt(*policy)) {
					slots[std::to_string(slot)] = *number;
				} else {
					slots[std::to_string(slot)] = *policy;
				}
			}
			if (!slots.empty()) {
				entry["slotPolicies"] = std::move(slots);
			}
			if (const auto blanket = PolicyToInt(outfit->blanket_policy())) {
				entry["slotPolicy"] = *blanket;
			} else {
				entry["slotPolicy"] = outfit->blanket_policy();
			}
		}

		const auto text = entry.dump(2);
		if (!SetClipboardText(text)) {
			SKSE::log::error("Outfits: could not write '{}' to the clipboard.", a_outfitName);
			return false;
		}
		SKSE::log::info("Outfits: copied '{}' to the clipboard ({} bytes of JSON).", a_outfitName, text.size());
		return true;
	}

	Result Paste(std::string& a_createdName)
	{
		a_createdName.clear();

		std::string text;
		bool        opened = false;
		if (!GetClipboardText(text, opened)) {
			return opened ? Result::ClipboardEmpty : Result::ClipboardOpenFail;
		}

		const auto root = nlohmann::json::parse(text, nullptr, false);
		if (root.is_discarded()) {
			return Result::InvalidJson;
		}
		nlohmann::json entry = root;
		if (root.is_object()) {
			if (const auto it = root.find("outfits"); it != root.end() && it->is_array() && !it->empty()) {
				entry = it->front();
			}
		}
		if (!entry.is_object()) {
			return Result::NoOutfit;
		}

		// The guard owns the store lock; binding inner() off a temporary would
		// release it at the end of this statement and leave the whole paste below
		// (name search, armour lookups, create_outfit_preset) running unlocked.
		auto  guard = outfit_service_get_mut_singleton_ptr();
		auto& service = guard->inner();
		const auto maxLength = static_cast<std::size_t>(std::max<std::int32_t>(1, service.max_outfit_name_len()));

		auto base = TrimName(ReadString(entry, { "name", "outfitName", "outfit_name" }));
		if (base.empty()) {
			base = kFallbackName;
		}
		if (base.size() > maxLength) {
			base = TruncateUtf8(base, maxLength);
		}
		const auto name = FreeName(service, base, maxLength);
		if (name.empty()) {
			return Result::NameTooLong;
		}

		std::vector<const RE::TESObjectARMO*> armors;
		if (const auto it = entry.find("armors"); it != entry.end() && it->is_array()) {
			armors.reserve(it->size());
			for (const auto& piece : *it) {
				if (const auto* armor = ResolveArmor(piece)) {
					armors.push_back(armor);
				}
			}
		}

		const auto blanket = ReadPolicy(entry, { "slotPolicy", "slot_policy" }).value_or(std::string("XXXX"));

		std::vector<std::pair<std::uint32_t, std::string>> slots;
		for (const char* key : { "slotPolicies", "slot_policies" }) {
			if (const auto it = entry.find(key); it != entry.end() && it->is_object()) {
				for (const auto& [slotName, value] : it->items()) {
					std::uint32_t slot = 0;
					try {
						slot = static_cast<std::uint32_t>(std::stoul(slotName));
					} catch (const std::exception&) {
						continue;
					}
					if (const auto code = ReadPolicyValue(value)) {
						slots.emplace_back(slot, *code);
					}
				}
				break;
			}
		}

		bool favorite = false;
		for (const char* key : { "isFavorite", "is_favorite" }) {
			if (const auto it = entry.find(key); it != entry.end() && it->is_boolean()) {
				favorite = it->get<bool>();
				break;
			}
		}
		if (!service.create_outfit_preset(name, armors, blanket, slots, favorite)) {
			return Result::CreateFailed;
		}
		SKSE::log::info("Outfits: pasted '{}' as '{}' ({} pieces, blanket {}, {} slot overrides).", base, name, armors.size(), blanket, slots.size());
		a_createdName = name;
		return Result::Ok;
	}
}
