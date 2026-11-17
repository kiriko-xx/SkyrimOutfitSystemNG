//
// Migration of co-save records written by the original (Rust/protobuf) mod.
//
// The original mod serialised a single protobuf message ("AAOS" record,
// versions 4, 5 and 6). This file decodes that message without pulling in a
// protobuf runtime: the schema is tiny and frozen, so a hand written reader
// keeps the plugin free of extra dependencies and lets existing saves survive
// the move to the C++ data layer.
//
// Decoding rules mirrored from the Rust implementation:
//   * ArmorLocator.form_id is a *runtime* form id -> ResolveFormID, with the
//     (local id, plugin name) pair as a fallback.
//   * versions 4 and 5 stored the slot policies as *numbers* (proto fields 6
//     and 7); version 6 switched to the policy names. Both are decoded into
//     the same in-memory representation, exactly like Policy::from_int does in
//     the original Rust code (0..11 = built-in table, 128+ = custom policy).
//   * versions below 6 keyed the actor map by handle, not by form id; those
//     assignments cannot be mapped onto actors reliably and are dropped
//     (mirrors "migration_save_v6", the outfits themselves are kept).
//

#include "core/OutfitService.h"

#include "core/Policy.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <map>
#include <utility>

namespace SOS
{
	namespace
	{
		struct LegacyArmor
		{
			std::uint32_t raw{ 0 };
			std::uint32_t local{ 0 };
			std::string mod;
		};

		struct LegacyOutfit
		{
			std::string name;
			bool favorite{ false };
			std::string blanket;
			std::map<std::uint32_t, std::string> slots;
			std::vector<LegacyArmor> armors;
			// Pre v6 fields: raw form ids instead of armor locators, and the
			// slot policies as small integers.
			std::vector<std::uint32_t> obsoleteArmors;
			std::map<std::uint32_t, std::uint32_t> obsoleteSlots;
			std::uint32_t obsoleteBlanket{ 0 };
		};

		struct LegacyAssignment
		{
			std::string current;
			bool autoSwitch{ false };
			std::map<std::uint32_t, std::string> states;
		};

		// Minimal protobuf wire format reader (varint, length delimited,
		// fixed32/fixed64 for skipping unknown fields).
		class ProtoReader
		{
		public:
			ProtoReader() = default;
			ProtoReader(const std::uint8_t* a_data, std::size_t a_size) :
				_data(a_data), _size(a_size) {}

			[[nodiscard]] bool AtEnd() const { return _pos >= _size; }

			bool ReadVarint(std::uint64_t& a_out)
			{
				std::uint64_t result = 0;
				int shift = 0;
				while (_pos < _size && shift < 64) {
					const std::uint8_t byte = _data[_pos++];
					result |= static_cast<std::uint64_t>(byte & 0x7F) << shift;
					if ((byte & 0x80) == 0) {
						a_out = result;
						return true;
					}
					shift += 7;
				}
				return false;
			}

			bool ReadTag(std::uint32_t& a_field, std::uint32_t& a_wire)
			{
				std::uint64_t tag = 0;
				if (!ReadVarint(tag)) {
					return false;
				}
				a_field = static_cast<std::uint32_t>(tag >> 3);
				a_wire = static_cast<std::uint32_t>(tag & 0x7);
				return a_field != 0;
			}

			bool ReadBytes(std::string& a_out)
			{
				std::uint64_t length = 0;
				if (!ReadVarint(length)) {
					return false;
				}
				if (length > _size - _pos) {
					return false;
				}
				a_out.assign(reinterpret_cast<const char*>(_data + _pos), static_cast<std::size_t>(length));
				_pos += static_cast<std::size_t>(length);
				return true;
			}

			bool ReadSub(ProtoReader& a_out)
			{
				std::uint64_t length = 0;
				if (!ReadVarint(length)) {
					return false;
				}
				if (length > _size - _pos) {
					return false;
				}
				a_out = ProtoReader(_data + _pos, static_cast<std::size_t>(length));
				_pos += static_cast<std::size_t>(length);
				return true;
			}

			bool Skip(std::uint32_t a_wire)
			{
				switch (a_wire) {
				case 0: {
					std::uint64_t value = 0;
					return ReadVarint(value);
				}
				case 1:
					if (_size - _pos < 8) {
						return false;
					}
					_pos += 8;
					return true;
				case 2: {
					std::string tmp;
					return ReadBytes(tmp);
				}
				case 5:
					if (_size - _pos < 4) {
						return false;
					}
					_pos += 4;
					return true;
				default:
					return false;
				}
			}

		private:
			const std::uint8_t* _data{ nullptr };
			std::size_t _size{ 0 };
			std::size_t _pos{ 0 };
		};

		bool ParseArmor(ProtoReader& a_reader, LegacyArmor& a_out)
		{
			while (!a_reader.AtEnd()) {
				std::uint32_t field = 0;
				std::uint32_t wire = 0;
				if (!a_reader.ReadTag(field, wire)) {
					return false;
				}
				if (field == 1 && wire == 0) {
					std::uint64_t value = 0;
					if (!a_reader.ReadVarint(value)) {
						return false;
					}
					a_out.raw = static_cast<std::uint32_t>(value);
					continue;
				}
				if (field == 2 && wire == 0) {
					std::uint64_t value = 0;
					if (!a_reader.ReadVarint(value)) {
						return false;
					}
					a_out.local = static_cast<std::uint32_t>(value);
					continue;
				}
				if (field == 3 && wire == 2) {
					if (!a_reader.ReadBytes(a_out.mod)) {
						return false;
					}
					continue;
				}
				if (!a_reader.Skip(wire)) {
					return false;
				}
			}
			return true;
		}

		bool ParseStringMap(ProtoReader& a_reader, std::map<std::uint32_t, std::string>& a_out)
		{
			std::uint32_t key = 0;
			std::string value;
			while (!a_reader.AtEnd()) {
				std::uint32_t field = 0;
				std::uint32_t wire = 0;
				if (!a_reader.ReadTag(field, wire)) {
					return false;
				}
				if (field == 1 && wire == 0) {
					std::uint64_t raw = 0;
					if (!a_reader.ReadVarint(raw)) {
						return false;
					}
					key = static_cast<std::uint32_t>(raw);
					continue;
				}
				if (field == 2 && wire == 2) {
					if (!a_reader.ReadBytes(value)) {
						return false;
					}
					continue;
				}
				if (!a_reader.Skip(wire)) {
					return false;
				}
			}
			if (!value.empty()) {
				a_out[key] = value;
			}
			return true;
		}

		bool ParseUint32Map(ProtoReader& a_reader, std::map<std::uint32_t, std::uint32_t>& a_out)
		{
			std::uint32_t key = 0;
			std::uint32_t value = 0;
			bool          hasKey = false;
			bool          hasValue = false;
			while (!a_reader.AtEnd()) {
				std::uint32_t field = 0;
				std::uint32_t wire = 0;
				if (!a_reader.ReadTag(field, wire)) {
					return false;
				}
				if (field == 1 && wire == 0) {
					std::uint64_t raw = 0;
					if (!a_reader.ReadVarint(raw)) {
						return false;
					}
					key = static_cast<std::uint32_t>(raw);
					hasKey = true;
					continue;
				}
				if (field == 2 && wire == 0) {
					std::uint64_t raw = 0;
					if (!a_reader.ReadVarint(raw)) {
						return false;
					}
					value = static_cast<std::uint32_t>(raw);
					hasValue = true;
					continue;
				}
				if (!a_reader.Skip(wire)) {
					return false;
				}
			}
			if (hasKey && hasValue) {
				a_out[key] = value;
			}
			return true;
		}

		bool ParseOutfit(ProtoReader& a_reader, LegacyOutfit& a_out)
		{
			while (!a_reader.AtEnd()) {
				std::uint32_t field = 0;
				std::uint32_t wire = 0;
				if (!a_reader.ReadTag(field, wire)) {
					return false;
				}
				switch (field) {
				case 1:
					if (wire != 2 || !a_reader.ReadBytes(a_out.name)) {
						return false;
					}
					break;
				case 2: {
					// repeated uint32 OBSOLETE_armors, packed by default but
					// older writers may emit one varint per tag.
					if (wire == 0) {
						std::uint64_t value = 0;
						if (!a_reader.ReadVarint(value)) {
							return false;
						}
						a_out.obsoleteArmors.push_back(static_cast<std::uint32_t>(value));
						break;
					}
					if (wire != 2) {
						return false;
					}
					ProtoReader packed;
					if (!a_reader.ReadSub(packed)) {
						return false;
					}
					while (!packed.AtEnd()) {
						std::uint64_t value = 0;
						if (!packed.ReadVarint(value)) {
							return false;
						}
						a_out.obsoleteArmors.push_back(static_cast<std::uint32_t>(value));
					}
					break;
				}
				case 3: {
					std::uint64_t value = 0;
					if (wire != 0 || !a_reader.ReadVarint(value)) {
						return false;
					}
					a_out.favorite = value != 0;
					break;
				}
				case 6: {
					if (wire != 2) {
						return false;
					}
					ProtoReader sub;
					if (!a_reader.ReadSub(sub)) {
						return false;
					}
					ParseUint32Map(sub, a_out.obsoleteSlots);
					break;
				}
				case 7: {
					std::uint64_t value = 0;
					if (wire != 0 || !a_reader.ReadVarint(value)) {
						return false;
					}
					a_out.obsoleteBlanket = static_cast<std::uint32_t>(value);
					break;
				}
				case 8: {
					if (wire != 2) {
						return false;
					}
					ProtoReader sub;
					if (!a_reader.ReadSub(sub)) {
						return false;
					}
					LegacyArmor armor;
					ParseArmor(sub, armor);
					a_out.armors.push_back(std::move(armor));
					break;
				}
				case 9:
					if (wire != 2 || !a_reader.ReadBytes(a_out.blanket)) {
						return false;
					}
					break;
				case 10: {
					if (wire != 2) {
						return false;
					}
					ProtoReader sub;
					if (!a_reader.ReadSub(sub)) {
						return false;
					}
					ParseStringMap(sub, a_out.slots);
					break;
				}
				default:
					if (!a_reader.Skip(wire)) {
						return false;
					}
					break;
				}
			}
			return true;
		}

		bool ParseAssignment(ProtoReader& a_reader, LegacyAssignment& a_out)
		{
			while (!a_reader.AtEnd()) {
				std::uint32_t field = 0;
				std::uint32_t wire = 0;
				if (!a_reader.ReadTag(field, wire)) {
					return false;
				}
				switch (field) {
				case 1:
					if (wire != 2 || !a_reader.ReadBytes(a_out.current)) {
						return false;
					}
					break;
				case 2: {
					if (wire != 2) {
						return false;
					}
					ProtoReader sub;
					if (!a_reader.ReadSub(sub)) {
						return false;
					}
					ParseStringMap(sub, a_out.states);
					break;
				}
				case 3: {
					std::uint64_t value = 0;
					if (wire != 0 || !a_reader.ReadVarint(value)) {
						return false;
					}
					a_out.autoSwitch = value != 0;
					break;
				}
				default:
					if (!a_reader.Skip(wire)) {
						return false;
					}
					break;
				}
			}
			return true;
		}

		RE::TESObjectARMO* ResolveLegacyRawArmor(std::uint32_t a_raw, const SKSE::SerializationInterface* a_intfc)
		{
			if (a_raw == 0 || !a_intfc) {
				return nullptr;
			}
			RE::FormID resolved = a_raw;
			if (!a_intfc->ResolveFormID(a_raw, resolved) || resolved == 0) {
				return nullptr;
			}
			return RE::TESForm::LookupByID<RE::TESObjectARMO>(resolved);
		}

		RE::TESObjectARMO* ResolveLegacyArmor(const LegacyArmor& a_armor, const SKSE::SerializationInterface* a_intfc)
		{
			if (a_armor.raw != 0 && a_intfc) {
				RE::FormID resolved = a_armor.raw;
				if (a_intfc->ResolveFormID(a_armor.raw, resolved) && resolved != 0) {
					if (auto* form = RE::TESForm::LookupByID<RE::TESObjectARMO>(resolved)) {
						return form;
					}
				}
			}
			if (a_armor.local != 0 && !a_armor.mod.empty()) {
				if (auto* handler = RE::TESDataHandler::GetSingleton()) {
					if (auto* form = handler->LookupForm<RE::TESObjectARMO>(a_armor.local, a_armor.mod)) {
						return form;
					}
				}
			}
			return nullptr;
		}
	}

	bool OutfitService::replace_with_legacy_proto(const void* a_data, std::size_t a_length, std::uint32_t a_version, const SKSE::SerializationInterface* a_intfc)
	{
		if (!a_data || a_length == 0) {
			return false;
		}

		std::lock_guard lock(_mutex);
		_outfits.clear();
		_assignments.clear();

		bool enabled = true;
		std::vector<LegacyOutfit> outfits;
		std::vector<std::pair<std::uint32_t, LegacyAssignment>> actors;

		ProtoReader root(static_cast<const std::uint8_t*>(a_data), a_length);
		while (!root.AtEnd()) {
			std::uint32_t field = 0;
			std::uint32_t wire = 0;
			if (!root.ReadTag(field, wire)) {
				break;
			}
			if (field == 1 && wire == 0) {
				std::uint64_t value = 0;
				if (!root.ReadVarint(value)) {
					break;
				}
				enabled = value != 0;
				continue;
			}
			if (field == 3 && wire == 2) {
				ProtoReader sub;
				if (!root.ReadSub(sub)) {
					break;
				}
				LegacyOutfit outfit;
				if (ParseOutfit(sub, outfit)) {
					outfits.push_back(std::move(outfit));
				}
				continue;
			}
			if (field == 7 && wire == 2) {
				ProtoReader entry;
				if (!root.ReadSub(entry)) {
					break;
				}
				std::uint32_t key = 0;
				LegacyAssignment value;
				while (!entry.AtEnd()) {
					std::uint32_t entryField = 0;
					std::uint32_t entryWire = 0;
					if (!entry.ReadTag(entryField, entryWire)) {
						break;
					}
					if (entryField == 1 && entryWire == 0) {
						std::uint64_t raw = 0;
						if (!entry.ReadVarint(raw)) {
							break;
						}
						key = static_cast<std::uint32_t>(raw);
						continue;
					}
					if (entryField == 2 && entryWire == 2) {
						ProtoReader sub;
						if (!entry.ReadSub(sub)) {
							break;
						}
						ParseAssignment(sub, value);
						continue;
					}
					if (!entry.Skip(entryWire)) {
						break;
					}
				}
				actors.emplace_back(key, std::move(value));
				continue;
			}
			if (!root.Skip(wire)) {
				break;
			}
		}

		_enabled = enabled;

		for (auto& legacy : outfits) {
			if (legacy.name.empty()) {
				continue;
			}
			auto* outfit = get_or_create_mut_outfit_ptr(legacy.name.c_str());
			if (!outfit) {
				continue;
			}
			outfit->set_favorite(legacy.favorite);
			// Both armor encodings are read, exactly like from_proto_data() of
			// the original: raw form ids first, then the locator entries.
			for (const auto raw : legacy.obsoleteArmors) {
				if (auto* form = ResolveLegacyRawArmor(raw, a_intfc)) {
					outfit->insert_armor(form);
				}
			}
			for (const auto& armor : legacy.armors) {
				if (auto* form = ResolveLegacyArmor(armor, a_intfc)) {
					outfit->insert_armor(form);
				}
			}

			// A migrated outfit does NOT start from the defaults a freshly
			// created outfit gets. The original builds it with an empty slot
			// map and the blanket policy "XXXX", then applies whatever the
			// save contains. Skipping this reset is what turned the old
			// "XXXO / require equipped" into "XXOO / always use outfit": the
			// numeric v4 policy was never read, so the constructor defaults
			// (blanket XXOO and a XEXO shield slot) survived.
			for (std::uint32_t slot = 0; slot < kSlotCount; ++slot) {
				outfit->set_slot_policy_raw(slot, std::nullopt);
			}
			outfit->set_blanket_policy_raw("XXXX");

			// Policy names (v6 and later).
			for (const auto& [slot, code] : legacy.slots) {
				if (slot < kSlotCount && !code.empty()) {
					outfit->set_slot_policy_raw(slot, code);
				}
			}
			if (!legacy.blanket.empty()) {
				outfit->set_blanket_policy_raw(legacy.blanket);
			}
			// Numeric policies (v4/v5) are applied last, they override the
			// named ones just like in the original implementation.
			for (const auto& [slot, value] : legacy.obsoleteSlots) {
				if (slot >= kSlotCount) {
					continue;
				}
				if (auto code = PolicyFromInt(value)) {
					outfit->set_slot_policy_raw(slot, std::move(*code));
				}
			}
			if (legacy.obsoleteBlanket != 0) {
				if (auto code = PolicyFromInt(legacy.obsoleteBlanket)) {
					outfit->set_blanket_policy_raw(std::move(*code));
				}
			}
		}

		if (a_version >= 6) {
			for (auto& [rawKey, legacy] : actors) {
				RE::FormID resolved = rawKey;
				if (a_intfc) {
					a_intfc->ResolveFormID(rawKey, resolved);
				}
				if (resolved == 0 || !IsFormIDPermitted(resolved)) {
					continue;
				}
				ActorAssignments assignment;
				assignment.stateBasedAutoSwitchingEnabled = legacy.autoSwitch;
				if (!legacy.current.empty()) {
					auto key = Canonical(legacy.current);
					if (_outfits.contains(key)) {
						assignment.current = key;
					}
				}
				for (const auto& [state, name] : legacy.states) {
					if (state >= kStateTypeCount) {
						continue;
					}
					auto key = Canonical(name);
					if (_outfits.contains(key)) {
						assignment.stateBased[state] = key;
					}
				}
				_assignments[resolved] = std::move(assignment);
			}
		} else if (!actors.empty()) {
			SKSE::log::warn("Co-save v{} stored {} actor assignments keyed by handle; they cannot be mapped onto actors and are dropped (the outfits are kept).", a_version, actors.size());
		}

		check_consistency();
		++_revision;
		SKSE::log::info("Migrated co-save v{}: {} outfits, {} actor assignments.", a_version, _outfits.size(), _assignments.size());
		return true;
	}
}// namespace SOS
