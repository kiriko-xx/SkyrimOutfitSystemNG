#pragma once

// Minimal C++ replacement for the cxx-generated "rust" types. The original
// mod kept its data model in Rust and exposed it through a cxx bridge;
// this port implements the same surface in plain C++ so that the Papyrus
// layer (OutfitSystem.cpp) and the hooks (Patches.cpp) stay byte-for-byte
// identical to the original implementation.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace RE
{
	class TESObjectARMO;
}

namespace rust
{
	class Str
	{
	public:
		Str() = default;
		Str(const char* a_data, std::size_t a_len) :
			_data(a_data), _len(a_len) {}
		Str(const char* a_data) :
			_data(a_data), _len(a_data ? std::strlen(a_data) : 0) {}
		Str(const std::string& a_str) :
			_data(a_str.data()), _len(a_str.size()) {}

		[[nodiscard]] const char* data() const { return _data ? _data : ""; }
		[[nodiscard]] std::size_t length() const { return _len; }
		[[nodiscard]] std::size_t size() const { return _len; }

		[[nodiscard]] std::string str() const { return std::string(data(), _len); }

	private:
		const char* _data{ nullptr };
		std::size_t _len{ 0 };
	};

	class String : public std::string
	{
	public:
		using std::string::string;
		String() = default;
		String(const std::string& a_rhs) :
			std::string(a_rhs) {}
		String(std::string&& a_rhs) :
			std::string(std::move(a_rhs)) {}
		String(const char* a_rhs) :
			std::string(a_rhs ? a_rhs : "") {}
	};

	template <class T>
	class Vec : public std::vector<T>
	{
	public:
		using std::vector<T>::vector;
		Vec() = default;
		Vec(const std::vector<T>& a_rhs) :
			std::vector<T>(a_rhs) {}
		Vec(std::vector<T>&& a_rhs) :
			std::vector<T>(std::move(a_rhs)) {}
	};

	template <class T>
	class Slice
	{
	public:
		Slice(T* a_data, std::size_t a_len) :
			_data(a_data), _len(a_len) {}

		[[nodiscard]] T* data() const { return _data; }
		[[nodiscard]] std::size_t size() const { return _len; }
		[[nodiscard]] std::size_t length() const { return _len; }
		[[nodiscard]] bool empty() const { return _len == 0; }
		[[nodiscard]] T& operator[](std::size_t a_index) const { return _data[a_index]; }
		[[nodiscard]] T* begin() const { return _data; }
		[[nodiscard]] T* end() const { return _data + _len; }

	private:
		T* _data{ nullptr };
		std::size_t _len{ 0 };
	};
}// namespace rust

// The 13 location states. The numeric values are part of the Papyrus API and
// of the co-save format, so they must never change.
enum class StateType : std::uint32_t
{
	World = 0,
	Town = 1,
	Dungeon = 2,
	WorldSnowy = 3,
	TownSnowy = 4,
	DungeonSnowy = 5,
	WorldRainy = 6,
	TownRainy = 7,
	DungeonRainy = 8,
	City = 9,
	CitySnowy = 10,
	CityRainy = 11,
	Combat = 12
};

inline constexpr std::uint32_t kStateTypeCount = 13;

struct WeatherFlags
{
	bool rainy{ false };
	bool snowy{ false };
};

struct OptionalStateType
{
	bool has_value{ false };
	StateType value{ StateType::World };
};

struct TESObjectARMOPtr
{
	const RE::TESObjectARMO* ptr{ nullptr };
};

struct MetadataC
{
	rust::String name;
	std::uint8_t sort_order{ 0 };
	bool advanced{ false };
};
