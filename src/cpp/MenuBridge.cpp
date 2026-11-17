#include "MenuBridge.h"

#include "ImGuiApi.h"

#include <Windows.h>

#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
	using AddSectionItemFn = void(__cdecl*)(const char*, SOS::MenuBridge::RenderFunction);
	using AddWindowFn = SOS::MenuBridge::WindowInterface* (__cdecl*)(SOS::MenuBridge::RenderFunction);

	HMODULE g_smf{ nullptr };
	AddSectionItemFn g_addSectionItem{ nullptr };
	AddWindowFn g_addWindow{ nullptr };
	std::atomic<bool> g_available{ false };
	std::wstring g_moduleDir;
	float g_frameworkVersion{ 0.0f };
	std::uint32_t g_frameworkApi{ 0 };
	bool g_hasBlockUserInput{ false };
	std::uint64_t g_inputRegistration{ 0 };

	std::wstring ComputeModuleDir()
	{
		HMODULE self{ nullptr };
		if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(&ComputeModuleDir), &self)) {
			return {};
		}

		wchar_t buffer[MAX_PATH]{};
		const DWORD length = ::GetModuleFileNameW(self, buffer, MAX_PATH);
		if (length == 0 || length == MAX_PATH) {
			return {};
		}

		return std::filesystem::path(std::wstring(buffer, length)).parent_path().wstring();
	}

	std::wstring ExecutableDir()
	{
		wchar_t buffer[MAX_PATH]{};
		const DWORD length = ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);
		if (length == 0 || length == MAX_PATH) {
			return {};
		}
		return std::filesystem::path(std::wstring(buffer, length)).parent_path().wstring();
	}

	std::vector<std::filesystem::path> SearchDirs()
	{
		std::vector<std::filesystem::path> dirs;

		const auto add = [&](const std::filesystem::path& a_dir) {
			if (a_dir.empty()) {
				return;
			}
			std::error_code ec;
			const auto canonical = std::filesystem::weakly_canonical(a_dir, ec);
			const auto& value = ec ? a_dir : canonical;
			for (const auto& existing : dirs) {
				if (existing == value) {
					return;
				}
			}
			dirs.push_back(value);
		};

		// Whichever copy of the framework is already in the process knows best
		// where its own siblings live.
		if (const HMODULE smf = ::GetModuleHandleW(L"SKSEMenuFramework.dll")) {
			wchar_t buffer[MAX_PATH]{};
			if (const DWORD length = ::GetModuleFileNameW(smf, buffer, MAX_PATH); length != 0 && length != MAX_PATH) {
				add(std::filesystem::path(std::wstring(buffer, length)).parent_path());
			}
		}

		add(g_moduleDir);
		add(std::filesystem::path(g_moduleDir) / ".." / ".." / "..");
		add(std::filesystem::path(ExecutableDir()) / "Data" / "SKSE" / "Plugins");
		add(ExecutableDir());

		return dirs;
	}

	bool ResolveEntryPoints()
	{
		g_addSectionItem = reinterpret_cast<AddSectionItemFn>(::GetProcAddress(g_smf, "AddSectionItem"));
		g_addWindow = reinterpret_cast<AddWindowFn>(::GetProcAddress(g_smf, "AddWindow"));
		if (!g_addSectionItem || !g_addWindow) {
			LOG(debug, "Menu: SKSEMenuFramework.dll does not export AddSectionItem/AddWindow (stale framework?)");
			return false;
		}

		// 2.x and 3.x look identical from the outside, so print what the host
		// reports; it makes user reports far easier to triage.
		using GetVersionFn = float (*)();
		using GetApiVersionFn = std::uint32_t (*)();
		const auto getVersion = reinterpret_cast<GetVersionFn>(::GetProcAddress(g_smf, "GetMenuFrameworkVersion"));
		const auto getApiVersion = reinterpret_cast<GetApiVersionFn>(::GetProcAddress(g_smf, "GetMenuFrameworkAPIVersion"));
		g_frameworkVersion = getVersion ? getVersion() : 0.0f;
		g_frameworkApi = getApiVersion ? getApiVersion() : 0u;
		// 3.x owns BlockUserInput. 2.x never reaches this code: its ImGui is
		// the C++ API and ImGuiApi::Bind() fails before UI::Register() calls
		// AddWindow. The extra field is therefore safe here, and it must be
		// armed even when a build does not export the version getter.
		g_hasBlockUserInput = true;
		LOG(info, "Menu: SKSE Menu Framework {} (api {})", g_frameworkVersion, g_frameworkApi);

		return true;
	}
}

namespace SOS::MenuBridge
{
	bool EnsureLoaded()
	{
		if (g_available.load(std::memory_order_acquire)) {
			return true;
		}

		if (g_moduleDir.empty()) {
			g_moduleDir = ComputeModuleDir();
		}

		if (g_smf) {
			const bool resolved = ResolveEntryPoints();
			g_available.store(resolved, std::memory_order_release);
			return resolved;
		}

		g_smf = ::GetModuleHandleW(L"SKSEMenuFramework.dll");
		if (g_smf) {
			LOG(debug, "Menu: SKSEMenuFramework.dll is already loaded");
		} else {
			const std::wstring fileName = L"SKSEMenuFramework.dll";
			const auto dirs = SearchDirs();
			for (const auto& dir : dirs) {
				const auto candidate = dir / fileName;
				std::error_code ec;
				if (!std::filesystem::exists(candidate, ec)) {
					continue;
				}
				LOG(debug, "Menu: trying to load {}", candidate.string());
				g_smf = ::LoadLibraryExW(candidate.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
				if (g_smf) {
					break;
				}
				LOG(debug, "Menu: load failed for {} (error {})", candidate.string(), ::GetLastError());
			}
		}

		if (!g_smf) {
			LOG(debug, "Menu: SKSE Menu Framework not found (searched {} folders)", SearchDirs().size());
			return false;
		}

		const bool resolved = ResolveEntryPoints();
		g_available.store(resolved, std::memory_order_release);
		if (resolved) {
			LOG(debug, "Menu: SKSE Menu Framework runtime resolved");
		}
		return g_available.load(std::memory_order_acquire);
	}

	bool Available()
	{
		return g_available.load(std::memory_order_acquire);
	}

	WindowInterface* AddWindow(RenderFunction a_render)
	{
		if (!g_available.load(std::memory_order_acquire) || !g_addWindow) {
			return nullptr;
		}

		auto* window = g_addWindow(a_render);
		if (window && g_hasBlockUserInput) {
			// The quick picker is modal: let the framework pause the game and
			// translate the gamepad into ImGui keys while it is open.
			window->BlockUserInput.store(true, std::memory_order_release);
		}
		return window;
	}

	void AddSectionItem(const char* a_path, RenderFunction a_render)
	{
		if (!g_available.load(std::memory_order_acquire) || !g_addSectionItem || !a_path) {
			return;
		}
		g_addSectionItem(a_path, a_render);
	}

	bool RegisterInputEvent(InputEventCallback a_callback)
	{
		if (!g_available.load(std::memory_order_acquire) || !g_smf || !a_callback) {
			return false;
		}
		if (g_inputRegistration != 0) {
			return true;
		}

		using RegisterInputEventFn = std::uint64_t (*)(InputEventCallback);
		auto registerFn = reinterpret_cast<RegisterInputEventFn>(::GetProcAddress(g_smf, "RegisterInpoutEvent"));
		if (!registerFn) {
			registerFn = reinterpret_cast<RegisterInputEventFn>(::GetProcAddress(g_smf, "RegisterInputEvent"));
		}
		if (!registerFn) {
			LOG(warn, "Menu: the framework has no input-event export; the quick picker will fall back to ImGui key state.");
			return false;
		}

		g_inputRegistration = registerFn(a_callback);
		if (g_inputRegistration == 0) {
			LOG(warn, "Menu: the framework refused the input-event callback; the quick picker will fall back to ImGui key state.");
			return false;
		}
		LOG(info, "Menu: input event callback registered (id {}).", g_inputRegistration);
		return true;
	}

	float FrameworkVersion()
	{
		return g_frameworkVersion;
	}

	std::wstring ModuleDirectory()
	{
		if (g_moduleDir.empty()) {
			g_moduleDir = ComputeModuleDir();
		}
		return g_moduleDir;
	}

	bool TextInputActive()
	{
		if (!g_available.load(std::memory_order_acquire)) {
			return false;
		}

		const auto& api = ImGuiApi::Get();
		if (!api.GetCurrentContext || !api.IsAnyItemActive) {
			return false;
		}
		if (!api.GetCurrentContext()) {
			return false;
		}
		return api.IsAnyItemActive();
	}
}
