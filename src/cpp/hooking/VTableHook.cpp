#include "hooking/VTableHook.h"

#include <cstring>
#include <memory>

#include <Windows.h>

namespace Hooking
{
	namespace
	{
		// A slice of memory the plugin may read: committed, no guard page, and fully
		// inside the region the query reports.
		bool Readable(std::uintptr_t a_address, std::size_t a_size)
		{
			MEMORY_BASIC_INFORMATION info{};
			if (::VirtualQuery(reinterpret_cast<LPCVOID>(a_address), &info, sizeof(info)) == 0) {
				return false;
			}
			if (info.State != MEM_COMMIT) {
				return false;
			}
			if ((info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
				return false;
			}
			const auto base = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
			return a_address >= base && a_address + a_size <= base + info.RegionSize;
		}

		bool Executable(std::uintptr_t a_address)
		{
			MEMORY_BASIC_INFORMATION info{};
			if (::VirtualQuery(reinterpret_cast<LPCVOID>(a_address), &info, sizeof(info)) == 0) {
				return false;
			}
			if (info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
				return false;
			}
			const auto protect = info.Protect & 0xFF;
			return protect == PAGE_EXECUTE || protect == PAGE_EXECUTE_READ || protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
		}
	}

	void* InstallVFunc(const char* a_name, std::uintptr_t a_table, std::size_t a_slot, void* a_hook, void** a_original)
	{
		if (a_original) {
			*a_original = nullptr;
		}
		if (a_table == 0) {
			LOG(err, "{}: the virtual table is not known on this build; the hook stays off.", a_name);
			return nullptr;
		}
		if (a_hook == nullptr) {
			LOG(err, "{}: no replacement was given; the hook stays off.", a_name);
			return nullptr;
		}

		const auto slot = a_table + a_slot * sizeof(void*);
		if (!Readable(a_table, (a_slot + 1) * sizeof(void*)) || !Readable(slot, sizeof(void*))) {
			LOG(err, "{}: the virtual table at 0x{:X} slot {} is not readable; the hook stays off.", a_name, a_table, a_slot);
			return nullptr;
		}

		void* prior = nullptr;
		std::memcpy(std::addressof(prior), reinterpret_cast<const void*>(slot), sizeof(prior));
		if (prior == nullptr || !Executable(reinterpret_cast<std::uintptr_t>(prior))) {
			LOG(err, "{}: slot {} of the virtual table at 0x{:X} does not hold a function; the hook stays off.", a_name, a_slot, a_table);
			return nullptr;
		}

		DWORD previousProtect = 0;
		if (::VirtualProtect(reinterpret_cast<LPVOID>(slot), sizeof(void*), PAGE_READWRITE, std::addressof(previousProtect)) == 0) {
			LOG(err, "{}: the virtual table at 0x{:X} cannot be made writable (error {}); the hook stays off.", a_name, a_table, ::GetLastError());
			return nullptr;
		}
		auto* const exchanged = ::InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(slot), a_hook, prior);
		DWORD restoreProtect = 0;
		::VirtualProtect(reinterpret_cast<LPVOID>(slot), sizeof(void*), previousProtect, std::addressof(restoreProtect));

		if (exchanged != prior) {
			LOG(warn, "{}: another plugin replaced slot {} of the virtual table at 0x{:X} first; this hook stays off so that plugin keeps working.", a_name, a_slot, a_table);
			return nullptr;
		}

		if (a_original) {
			*a_original = prior;
		}
		LOG(info, "{}: slot {} of the virtual table at 0x{:X} is hooked (was 0x{:X}).", a_name, a_slot, a_table, reinterpret_cast<std::uintptr_t>(prior));
		return prior;
	}
}
