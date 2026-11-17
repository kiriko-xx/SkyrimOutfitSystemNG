#include "compat/ModuleInfo.h"

#include <SKSE/SKSE.h>

#include <vector>

namespace SOS::Compat
{
	std::string ModulePath(HMODULE a_module)
	{
		if (!a_module) {
			return {};
		}
		wchar_t buffer[MAX_PATH]{};
		const auto length = ::GetModuleFileNameW(a_module, buffer, MAX_PATH);
		if (length == 0) {
			return {};
		}
		const std::wstring wide(buffer, length);
		return std::string(wide.begin(), wide.end());
	}

	std::string ModuleVersion(HMODULE a_module)
	{
		const auto path = ModulePath(a_module);
		if (path.empty()) {
			return {};
		}
		DWORD ignored{ 0 };
		const auto size = ::GetFileVersionInfoSizeA(path.c_str(), &ignored);
		if (size == 0) {
			return {};
		}
		std::vector<std::uint8_t> data(size);
		if (!::GetFileVersionInfoA(path.c_str(), 0, size, data.data())) {
			return {};
		}
		VS_FIXEDFILEINFO* info{ nullptr };
		UINT length{ 0 };
		if (!::VerQueryValueA(data.data(), "\\", reinterpret_cast<void**>(&info), &length) || !info) {
			return {};
		}
		return fmt::format("{}.{}.{}.{}",
			HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
			HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
	}
}
