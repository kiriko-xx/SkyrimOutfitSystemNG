#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <optional>

namespace Hooking
{
	// Observation only: the caller supplies five bytes it has safely read from a_site.
	// No memory reads, executable-range validation, patch approval or thunk following.
	class CallSiteSnapshot
	{
	public:
		using Bytes = std::array<std::uint8_t, 5>;

		constexpr CallSiteSnapshot(std::uintptr_t a_site, const Bytes& a_bytes) noexcept :
			_site(a_site), _bytes(a_bytes)
		{}

		[[nodiscard]] constexpr const Bytes& OriginalBytes() const noexcept { return _bytes; }

		// Equality is not an atomic live-memory check or permission to overwrite code.
		[[nodiscard]] constexpr bool MatchesOriginal(const Bytes& a_bytes) const noexcept
		{
			return _bytes == a_bytes;
		}

		[[nodiscard]] constexpr std::optional<std::uintptr_t> ResumeAddress() const noexcept
		{
			if (_site > (std::numeric_limits<std::uintptr_t>::max)() - _bytes.size()) {
				return std::nullopt;
			}
			return _site + _bytes.size();
		}

		// Decode only E8 rel32's immediate destination, never its ultimate callee.
		[[nodiscard]] constexpr std::optional<std::uintptr_t> DirectTarget() const noexcept
		{
			if (_bytes[0] != 0xE8) {
				return std::nullopt;
			}
			const auto resume = ResumeAddress();
			if (!resume) {
				return std::nullopt;
			}
			const auto encoded = std::uint32_t{ _bytes[1] } |
			                     (std::uint32_t{ _bytes[2] } << 8) |
			                     (std::uint32_t{ _bytes[3] } << 16) |
			                     (std::uint32_t{ _bytes[4] } << 24);
			// Explicit sign extension avoids unaligned loads, host-endian assumptions,
			// implementation-defined unsigned-to-signed conversion and signed overflow.
			const auto displacement = static_cast<std::int64_t>(encoded) -
			                          ((encoded & 0x80000000u) ? 0x100000000LL : 0LL);
			if (displacement < 0) {
				const auto magnitude = static_cast<std::uint64_t>(-displacement);
				if (magnitude > *resume) {
					return std::nullopt;
				}
				return *resume - static_cast<std::uintptr_t>(magnitude);
			}
			const auto magnitude = static_cast<std::uint64_t>(displacement);
			if (magnitude > (std::numeric_limits<std::uintptr_t>::max)() - *resume) {
				return std::nullopt;
			}
			return *resume + static_cast<std::uintptr_t>(magnitude);
		}

	private:
		std::uintptr_t _site;
		Bytes _bytes;
	};
}
