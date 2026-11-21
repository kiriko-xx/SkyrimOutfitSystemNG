#include "core/GoldenItems.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "core/PluginScan.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

namespace SOS::Golden
{
	namespace
	{
		struct Entry
		{
			std::string   plugin;   // as stored, e.g. "Example.esp"
			std::uint32_t localId{ 0 };
		};

		struct Snapshot
		{
			std::vector<Entry>                          entries;  // de-duplicated, sorted
			std::shared_ptr<const std::unordered_set<RE::FormID>> resolved;
		};

		// Recursive on purpose: every public entry point of this file takes this
		// lock, and some of them call one another (ImportText -> counts,
		// SaveToDisk / LoadFromDisk -> counts). On MSVC a plain std::mutex throws
		// std::system_error("resource deadlock would occur") for that re-entry, and
		// an exception escaping the SKSE message handler takes the game down:
		// 1.5.40 crashed on start-up in LoadFromDisk -> ImportText for exactly this.
		std::recursive_mutex        g_mutex;
		std::shared_ptr<const Snapshot> g_snapshot;

		std::string LowerCopy(std::string_view a_text)
		{
			std::string out;
			out.reserve(a_text.size());
			for (const char c : a_text) {
				out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
			}
			return out;
		}

		bool SameEntry(const Entry& a_lhs, const Entry& a_rhs)
		{
			return a_lhs.localId == a_rhs.localId && LowerCopy(a_lhs.plugin) == LowerCopy(a_rhs.plugin);
		}

		bool EntryLess(const Entry& a_lhs, const Entry& a_rhs)
		{
			const auto lhs = LowerCopy(a_lhs.plugin);
			const auto rhs = LowerCopy(a_rhs.plugin);
			if (lhs != rhs) {
				return lhs < rhs;
			}
			return a_lhs.localId < a_rhs.localId;
		}

		// Normalises and sorts the entry list. Runs under g_mutex.
		void Normalise(std::vector<Entry>& a_entries)
		{
			std::sort(a_entries.begin(), a_entries.end(), EntryLess);
			a_entries.erase(std::unique(a_entries.begin(), a_entries.end(), SameEntry), a_entries.end());
		}

		std::shared_ptr<const std::unordered_set<RE::FormID>> Resolve(const std::vector<Entry>& a_entries)
		{
			auto resolved = std::make_shared<std::unordered_set<RE::FormID>>();
			resolved->reserve(a_entries.size());
			auto* handler = RE::TESDataHandler::GetSingleton();
			if (!handler) {
				return resolved;
			}
			std::size_t missing = 0;
			for (const auto& entry : a_entries) {
				auto* form = handler->LookupForm(entry.localId, entry.plugin);
				if (form) {
					resolved->insert(form->GetFormID());
				} else {
					++missing;
					SKSE::log::warn("Golden items: {}:{:08X} was not found in this load order.", entry.plugin, entry.localId);
				}
			}
			SKSE::log::debug("Golden items: {} entr(ies) resolved, {} missing.", resolved->size(), missing);
			return resolved;
		}

		// Publishes a new snapshot built from a_entries. Runs under g_mutex.
		void Publish(std::vector<Entry> a_entries)
		{
			Normalise(a_entries);
			auto snapshot = std::make_shared<Snapshot>();
			snapshot->resolved = Resolve(a_entries);
			snapshot->entries = std::move(a_entries);
			g_snapshot = std::move(snapshot);
		}

		const std::shared_ptr<const Snapshot>& Current()
		{
			if (!g_snapshot) {
				Publish({});
			}
			return g_snapshot;
		}

		// Counts read without taking g_mutex again. Use these from any code path
		// that already holds the lock; the public TotalCount()/GroupCount() stay
		// for callers outside this file.
		std::size_t TotalCountLocked()
		{
			return Current()->entries.size();
		}

		std::string FormatEntry(const Entry& a_entry)
		{
			char buffer[32]{};
			std::snprintf(buffer, sizeof(buffer), "%s:%08X", a_entry.plugin.c_str(), a_entry.localId);
			return std::string{ buffer };
		}

		std::string JsonText()  // caller holds g_mutex
		{
			nlohmann::json root;
			root["version"] = 1;
			root["groups"] = nlohmann::json::array();
			std::string current;
			nlohmann::json items = nlohmann::json::array();
			const auto flush = [&]() {
				if (current.empty()) {
					return;
				}
				nlohmann::json group;
				group["plugin"] = current;
				group["items"] = items;
				root["groups"].push_back(std::move(group));
				items = nlohmann::json::array();
			};
			for (const auto& entry : Current()->entries) {
				if (LowerCopy(entry.plugin) != LowerCopy(current)) {
					flush();
					current = entry.plugin;
				}
				char buffer[16]{};
				std::snprintf(buffer, sizeof(buffer), "%06X", entry.localId);
				items.push_back(std::string{ buffer });
			}
			flush();
			return root.dump(2);
		}

		bool ReadNumber(const nlohmann::json& a_item, std::uint32_t& a_out)
		{
			if (a_item.is_number_unsigned() || a_item.is_number_integer()) {
				a_out = static_cast<std::uint32_t>(a_item.get<std::int64_t>());
				return true;
			}
			if (a_item.is_string()) {
				std::string text = a_item.get<std::string>();
				if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
					text = text.substr(2);
				}
				if (text.empty()) {
					return false;
				}
				try {
					a_out = static_cast<std::uint32_t>(std::stoul(text, nullptr, 16));
					return true;
				} catch (...) {
					return false;
				}
			}
			return false;
		}
	}// namespace

	std::string JsonPath()
	{
		std::error_code ec;
		const auto root = std::filesystem::current_path(ec);
		if (ec) {
			return "OutfitSystemGoldenItems.json";
		}
		return (root / "Data" / "SKSE" / "Plugins" / "OutfitSystemGoldenItems.json").string();
	}

	void WarmUp()
	{
		std::lock_guard lock(g_mutex);
		Publish(Current()->entries);
	}

	bool IsGolden(const RE::TESObjectARMO* a_armor)
	{
		if (!a_armor) {
			return false;
		}
		std::shared_ptr<const Snapshot> snapshot;
		{
			std::lock_guard lock(g_mutex);
			snapshot = Current();
		}
		return snapshot && snapshot->resolved && snapshot->resolved->contains(a_armor->GetFormID());
	}

	std::vector<Group> Groups()
	{
		std::lock_guard lock(g_mutex);
		const auto& entries = Current()->entries;
		std::vector<Group> groups;
		std::string current;
		for (const auto& entry : entries) {
			if (LowerCopy(entry.plugin) != LowerCopy(current)) {
				Group group;
				group.plugin = entry.plugin;
				groups.push_back(std::move(group));
				current = entry.plugin;
			}
			groups.back().localIds.push_back(entry.localId);
		}
		return groups;
	}

	std::size_t GroupCount()
	{
		std::lock_guard lock(g_mutex);
		const auto& entries = Current()->entries;
		std::size_t count = 0;
		std::string current;
		for (const auto& entry : entries) {
			if (LowerCopy(entry.plugin) != LowerCopy(current)) {
				current = entry.plugin;
				++count;
			}
		}
		return count;
	}

	std::size_t TotalCount()
	{
		std::lock_guard lock(g_mutex);
		return Current()->entries.size();
	}

	std::size_t ApplyLegacyEntries(const std::vector<std::pair<std::string, std::uint32_t>>& a_entries)
	{
		if (a_entries.empty()) {
			return 0;
		}
		std::lock_guard lock(g_mutex);
		auto entries = Current()->entries;
		std::size_t added = 0;
		for (const auto& [plugin, localId] : a_entries) {
			if (plugin.empty()) {
				continue;
			}
			Entry entry;
			entry.plugin = plugin;
			entry.localId = localId;
			const bool known = std::any_of(entries.begin(), entries.end(), [&](const Entry& a_existing) {
				return SameEntry(a_existing, entry);
			});
			if (known) {
				continue;
			}
			entries.push_back(std::move(entry));
			++added;
		}
		if (added > 0) {
			Publish(std::move(entries));
			SKSE::log::info("Golden items: {} entr(ies) lifted out of the legacy ini list into the golden items store.", added);
		}
		return added;
	}

	bool AddArmor(const RE::TESObjectARMO* a_armor)
	{
		if (!a_armor) {
			return false;
		}
		const std::string plugin = PluginScan::OwningPlugin(a_armor);
		if (plugin.empty()) {
			return false;
		}
		const RE::FormID full = a_armor->GetFormID();
		const std::uint32_t localId = full & 0x00FFFFFFu;  // ESL ids keep the 12 bit index in the low word
		Entry entry;
		entry.plugin = plugin;
		entry.localId = localId;
		std::lock_guard lock(g_mutex);
		auto entries = Current()->entries;
		if (std::any_of(entries.begin(), entries.end(), [&](const Entry& a_existing) { return SameEntry(a_existing, entry); })) {
			return false;
		}
		const std::string formatted = FormatEntry(entry);
		entries.push_back(std::move(entry));
		Publish(std::move(entries));
		SKSE::log::info("Golden items: {} was added as a golden item.", formatted);
		return true;
	}

	bool AddLocal(const std::string& a_plugin, std::uint32_t a_localId, std::string& a_message)
	{
		if (a_plugin.empty()) {
			a_message = "The plugin name is empty.";
			return false;
		}
		auto* armor = PluginScan::ResolveArmor(a_plugin, a_localId);
		if (!armor) {
			a_message = "No slot armour with that id was found in that plugin.";
			return false;
		}
		const std::string owner = PluginScan::OwningPlugin(armor);
		Entry entry;
		entry.plugin = owner.empty() ? a_plugin : owner;
		entry.localId = a_localId & 0x00FFFFFFu;
		{
			std::lock_guard lock(g_mutex);
			auto entries = Current()->entries;
			if (std::any_of(entries.begin(), entries.end(), [&](const Entry& a_existing) { return SameEntry(a_existing, entry); })) {
				a_message = "That piece is already in the list.";
				return false;
			}
			entries.push_back(entry);
			Publish(std::move(entries));
		}
		a_message = FormatEntry(entry);
		SKSE::log::info("Golden items: {} was added as a golden item.", a_message);
		return true;
	}

	std::size_t AddAllFromPlugin(const std::string& a_plugin)
	{
		const auto armors = PluginScan::ArmorsOf(a_plugin);
		if (armors.empty()) {
			return 0;
		}
		std::lock_guard lock(g_mutex);
		auto entries = Current()->entries;
		std::size_t added = 0;
		for (const auto* armor : armors) {
			if (!armor) {
				continue;
			}
			Entry entry;
			entry.plugin = PluginScan::OwningPlugin(armor);
			if (entry.plugin.empty()) {
				continue;
			}
			entry.localId = armor->GetFormID() & 0x00FFFFFFu;
			const bool known = std::any_of(entries.begin(), entries.end(), [&](const Entry& a_existing) {
				return SameEntry(a_existing, entry);
			});
			if (known) {
				continue;
			}
			entries.push_back(std::move(entry));
			++added;
		}
		if (added > 0) {
			Publish(std::move(entries));
		}
		SKSE::log::info("Golden items: {} piece(s) of {} added.", added, a_plugin);
		return added;
	}

	bool RemoveLocal(const std::string& a_plugin, std::uint32_t a_localId)
	{
		Entry entry;
		entry.plugin = a_plugin;
		entry.localId = a_localId;
		std::lock_guard lock(g_mutex);
		auto entries = Current()->entries;
		const auto before = entries.size();
		entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const Entry& a_existing) {
			return SameEntry(a_existing, entry);
		}), entries.end());
		if (entries.size() == before) {
			return false;
		}
		Publish(std::move(entries));
		return true;
	}

	bool RemoveGroup(const std::string& a_plugin)
	{
		if (a_plugin.empty()) {
			return false;
		}
		const auto lower = LowerCopy(a_plugin);
		std::lock_guard lock(g_mutex);
		auto entries = Current()->entries;
		const auto before = entries.size();
		entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const Entry& a_existing) {
			return LowerCopy(a_existing.plugin) == lower;
		}), entries.end());
		if (entries.size() == before) {
			return false;
		}
		Publish(std::move(entries));
		SKSE::log::info("Golden items: the {} group was removed ({} piece(s)).", a_plugin, before - entries.size());
		return true;
	}

	bool LoadFromDisk()
	{
		const auto path = JsonPath();
		std::error_code ec;
		if (!std::filesystem::exists(path, ec)) {
			SKSE::log::debug("Golden items: no store at {} yet; starting empty.", path);
			return false;
		}
		std::ifstream in(path, std::ios::binary);
		if (!in) {
			SKSE::log::warn("Golden items: cannot read {}.", path);
			return false;
		}
		std::stringstream buffer;
		buffer << in.rdbuf();
		std::string message;
		if (!ImportText(buffer.str(), message)) {
			SKSE::log::warn("Golden items: {} could not be read: {}", path, message);
			return false;
		}
		SKSE::log::info("Golden items: {} entr(ies) loaded from {}.", TotalCountLocked(), path);
		return true;
	}

	bool SaveToDisk()
	{
		const auto path = JsonPath();
		std::error_code ec;
		const auto parent = std::filesystem::path{ path }.parent_path();
		if (!parent.empty()) {
			std::filesystem::create_directories(parent, ec);
		}
		std::string text;
		{
			std::lock_guard lock(g_mutex);
			text = JsonText();
		}
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		if (!out) {
			SKSE::log::warn("Golden items: cannot write {}.", path);
			return false;
		}
		out << text;
		out.close();
		SKSE::log::debug("Golden items: {} entr(ies) saved to {}.", TotalCountLocked(), path);
		return true;
	}

	std::string ExportText()
	{
		std::lock_guard lock(g_mutex);
		return JsonText();
	}

	bool ImportText(const std::string& a_json, std::string& a_message)
	{
		if (a_json.empty()) {
			a_message = "The document is empty.";
			return false;
		}
		nlohmann::json root;
		try {
			root = nlohmann::json::parse(a_json);
		} catch (const std::exception& e) {
			a_message = std::string("The document is not valid JSON: ") + e.what();
			return false;
		}
		std::vector<Entry> imported;
		const auto push = [&](const std::string& a_plugin, std::uint32_t a_localId) {
			if (a_plugin.empty()) {
				return;
			}
			Entry entry;
			entry.plugin = a_plugin;
			entry.localId = a_localId & 0x00FFFFFFu;
			imported.push_back(std::move(entry));
		};
		const auto readItem = [&](const nlohmann::json& a_item, const std::string& a_plugin) {
			if (a_item.is_object()) {
				const auto plugin = a_item.value("plugin", a_plugin);
				std::uint32_t localId = 0;
				if (a_item.contains("id") && ReadNumber(a_item.at("id"), localId)) {
					push(plugin, localId);
				} else if (a_item.contains("localId") && ReadNumber(a_item.at("localId"), localId)) {
					push(plugin, localId);
				}
				return;
			}
			std::uint32_t localId = 0;
			if (ReadNumber(a_item, localId)) {
				push(a_plugin, localId);
			}
		};
		if (root.is_object() && root.contains("groups") && root.at("groups").is_array()) {
			for (const auto& group : root.at("groups")) {
				if (!group.is_object()) {
					continue;
				}
				const auto plugin = group.value("plugin", std::string{});
				if (!group.contains("items") || !group.at("items").is_array()) {
					continue;
				}
				for (const auto& item : group.at("items")) {
					readItem(item, plugin);
				}
			}
		} else if (root.is_array()) {
			// Also accept a bare array: [{"plugin":"a.esp","id":"001234"}, ...]
			for (const auto& item : root) {
				readItem(item, std::string{});
			}
		} else {
			a_message = "The document has no \"groups\" list.";
			return false;
		}
		if (imported.empty()) {
			a_message = "The document contains no golden item that could be read.";
			return false;
		}
		std::lock_guard lock(g_mutex);
		Publish(std::move(imported));
		a_message = "Imported " + std::to_string(TotalCountLocked()) + " piece(s).";
		SKSE::log::info("Golden items: {} entr(ies) imported.", TotalCountLocked());
		return true;
	}
}
