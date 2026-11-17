#include "ImGuiApi.h"

#include <Windows.h>
#include <TlHelp32.h>

#include <initializer_list>
#include <string>
#include <vector>

namespace SOS::ImGuiApi
{
	namespace
	{
		Api g_api{};
		std::string g_host;
		std::string g_status{ "cimgui entry points not resolved yet" };
		bool g_failureLogged = false;

		struct Binding final
		{
			const char* name;
			void** slot;
		};

		std::string Narrow(const wchar_t* a_wide)
		{
			std::string out;
			if (!a_wide) {
				return out;
			}
			for (const wchar_t* it = a_wide; *it; ++it) {
				out.push_back(*it < 0x80 ? static_cast<char>(*it) : '?');
			}
			return out;
		}

		struct ModuleRef final
		{
			HMODULE handle;
			std::string name;
		};

		std::vector<ModuleRef> CandidateModules()
		{
			std::vector<ModuleRef> modules;

			const HMODULE framework = ::GetModuleHandleW(L"SKSEMenuFramework.dll");
			if (framework) {
				modules.push_back({ framework, "SKSEMenuFramework.dll" });
			}

			if (const HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, ::GetCurrentProcessId());
				snapshot != INVALID_HANDLE_VALUE) {
				MODULEENTRY32W entry{};
				entry.dwSize = sizeof(entry);
				if (::Module32FirstW(snapshot, &entry)) {
					do {
						if (entry.hModule != nullptr && entry.hModule != framework) {
							modules.push_back({ entry.hModule, Narrow(entry.szModule) });
						}
					} while (::Module32NextW(snapshot, &entry));
				}
				::CloseHandle(snapshot);
			}

			return modules;
		}

		// Every entry point has to exist before the table is committed: a
		// partially filled table would crash the first time a panel touches a
		// widget that is missing.
		bool TryBindModule(HMODULE a_module, const std::string& a_label, std::string& a_reason)
		{
			Api table{};

			#define SOS_BIND(a_field, a_symbol) Binding{ a_symbol, reinterpret_cast<void**>(&table.a_field) }
			const Binding bindings[]{
				SOS_BIND(GetCurrentContext, "igGetCurrentContext"),
				SOS_BIND(IsAnyItemActive, "igIsAnyItemActive"),
				SOS_BIND(SameLine, "igSameLine"),
				SOS_BIND(Spacing, "igSpacing"),
				SOS_BIND(Separator, "igSeparator"),
				SOS_BIND(SeparatorText, "igSeparatorText"),
				SOS_BIND(SetNextItemWidth, "igSetNextItemWidth"),
				SOS_BIND(TextV, "igTextV"),
				SOS_BIND(TextUnformatted, "igTextUnformatted"),
				SOS_BIND(TextDisabledV, "igTextDisabledV"),
				SOS_BIND(TextWrappedV, "igTextWrappedV"),
				SOS_BIND(SetTooltipV, "igSetTooltipV"),
				SOS_BIND(Button, "igButton"),
				SOS_BIND(SmallButton, "igSmallButton"),
				SOS_BIND(Checkbox, "igCheckbox"),
				SOS_BIND(Selectable, "igSelectable_Bool"),
				SOS_BIND(BeginCombo, "igBeginCombo"),
				SOS_BIND(EndCombo, "igEndCombo"),
				SOS_BIND(BeginChild, "igBeginChild_Str"),
				SOS_BIND(EndChild, "igEndChild"),
				SOS_BIND(TreeNode, "igTreeNode_Str"),
				SOS_BIND(TreePop, "igTreePop"),
				SOS_BIND(CollapsingHeader, "igCollapsingHeader_TreeNodeFlags"),
				SOS_BIND(SliderInt, "igSliderInt"),
				SOS_BIND(InputInt, "igInputInt"),
				SOS_BIND(InputText, "igInputText"),
				SOS_BIND(InputTextWithHint, "igInputTextWithHint"),
				SOS_BIND(PushIDInt, "igPushID_Int"),
				SOS_BIND(PushIDPtr, "igPushID_Ptr"),
				SOS_BIND(PopID, "igPopID"),
				SOS_BIND(IsItemHovered, "igIsItemHovered"),
				SOS_BIND(IsMouseDoubleClicked, "igIsMouseDoubleClicked_Nil")
			};
			#undef SOS_BIND

			constexpr std::size_t total = std::size(bindings);
			std::size_t resolved = 0;
			for (const auto& binding : bindings) {
				*binding.slot = reinterpret_cast<void*>(::GetProcAddress(a_module, binding.name));
				if (*binding.slot == nullptr) {
					a_reason = std::string("missing ") + binding.name + " in " + a_label + " (" +
					           std::to_string(resolved) + "/" + std::to_string(total) + " resolved)";
					return false;
				}
				++resolved;
			}

			// Optional helpers. A missing one only disables the feature that uses
			// it, never the binding itself: cimgui renamed several exports
			// between framework builds (igSetScrollHereY vs
			// igSetScrollHereY_Float), and the quick picker has to open even
			// when only the window four are available.
			const auto resolve = [a_module](std::initializer_list<const char*> a_names) -> void* {
				for (const char* name : a_names) {
					if (const auto proc = ::GetProcAddress(a_module, name)) {
						return reinterpret_cast<void*>(proc);
					}
				}
				return nullptr;
			};

			const auto bind_optional = [&](const char* a_field, void** a_slot, std::initializer_list<const char*> a_names) {
				*a_slot = resolve(a_names);
				if (*a_slot == nullptr) {
					std::string tried;
					for (const char* name : a_names) {
						if (!tried.empty()) {
							tried += ", ";
						}
						tried += name;
					}
					SKSE::log::warn("ImGuiApi: optional entry point {} is missing in {} (tried {}).", a_field, a_label, tried);
				}
				return *a_slot != nullptr;
			};

			std::size_t extrasTotal = 0;
			std::size_t extrasResolved = 0;
			const auto  count = [&](bool a_ok) {
				++extrasTotal;
				if (a_ok) {
					++extrasResolved;
				}
				return a_ok;
			};

			count(bind_optional("Begin", reinterpret_cast<void**>(&table.extras.Begin), { "igBegin" }));
			count(bind_optional("End", reinterpret_cast<void**>(&table.extras.End), { "igEnd" }));
			count(bind_optional("SetNextWindowPos", reinterpret_cast<void**>(&table.extras.SetNextWindowPos), { "igSetNextWindowPos" }));
			count(bind_optional("SetNextWindowSize", reinterpret_cast<void**>(&table.extras.SetNextWindowSize), { "igSetNextWindowSize" }));
			count(bind_optional("GetMainViewport", reinterpret_cast<void**>(&table.extras.GetMainViewport), { "igGetMainViewport" }));
			count(bind_optional("GetCurrentWindow", reinterpret_cast<void**>(&table.extras.GetCurrentWindow), { "igGetCurrentWindow" }));
			count(bind_optional("IsKeyDown", reinterpret_cast<void**>(&table.extras.IsKeyDown), { "igIsKeyDown_Nil", "igIsKeyDown" }));
			count(bind_optional("IsKeyPressed", reinterpret_cast<void**>(&table.extras.IsKeyPressed), { "igIsKeyPressed_Bool", "igIsKeyPressed" }));
			count(bind_optional("GetContentRegionAvail", reinterpret_cast<void**>(&table.extras.GetContentRegionAvail), { "igGetContentRegionAvail" }));
			count(bind_optional("CalcTextSize", reinterpret_cast<void**>(&table.extras.CalcTextSize), { "igCalcTextSize" }));
			count(bind_optional("GetScrollY", reinterpret_cast<void**>(&table.extras.GetScrollY), { "igGetScrollY" }));
			count(bind_optional("SetScrollHereY", reinterpret_cast<void**>(&table.extras.SetScrollHereY), { "igSetScrollHereY_Float", "igSetScrollHereY" }));
			count(bind_optional("IsItemActivated", reinterpret_cast<void**>(&table.extras.IsItemActivated), { "igIsItemActivated" }));
			count(bind_optional("Dummy", reinterpret_cast<void**>(&table.extras.Dummy), { "igDummy" }));

			// Geometry pair, deliberately outside of the extras group: without
			// it the quick picker simply opens centred again on the next start
			// instead of remembering where the user put it.
			table.extras.GetWindowPos = reinterpret_cast<decltype(table.extras.GetWindowPos)>(
				::GetProcAddress(a_module, "igGetWindowPos"));
			table.extras.GetWindowSize = reinterpret_cast<decltype(table.extras.GetWindowSize)>(
				::GetProcAddress(a_module, "igGetWindowSize"));
			table.hasWindowRect = table.extras.GetWindowPos != nullptr && table.extras.GetWindowSize != nullptr;
			if (!table.hasWindowRect) {
				SKSE::log::warn("ImGuiApi: igGetWindowPos/igGetWindowSize are missing in {}; the quick picker "
								"will not remember its position.",
					a_label);
			}

			// Item flags are optional too: without them the quick picker leaves
			// ImGui's own navigation enabled (old behaviour), but still opens.
			table.PushItemFlag = reinterpret_cast<decltype(table.PushItemFlag)>(
				::GetProcAddress(a_module, "igPushItemFlag"));
			table.PopItemFlag = reinterpret_cast<decltype(table.PopItemFlag)>(
				::GetProcAddress(a_module, "igPopItemFlag"));
			table.hasItemFlags = table.PushItemFlag != nullptr && table.PopItemFlag != nullptr;
			if (!table.hasItemFlags) {
				SKSE::log::warn("ImGuiApi: igPushItemFlag/igPopItemFlag are missing in {}; ImGui navigation stays enabled in the quick picker.",
					a_label);
			}

			table.hasExtras = extrasTotal == extrasResolved;
			table.hasWindowPlumbing = table.extras.Begin && table.extras.End &&
				table.extras.SetNextWindowPos && table.extras.SetNextWindowSize;
			if (!table.hasWindowPlumbing) {
				SKSE::log::error("ImGuiApi: the window entry points are incomplete in {}; the quick picker "
								 "cannot open as a standalone window.",
					a_label);
			} else {
				SKSE::log::debug("ImGuiApi: {}/{} optional helpers resolved in {}; window geometry {}.", extrasResolved,
					extrasTotal, a_label, table.hasWindowRect ? "tracked" : "not tracked");
			}

			g_api = table;
			g_host = a_label;
			return true;
		}
	}

	Api& Get()
	{
		return g_api;
	}

	bool Bind()
	{
		if (g_api.GetCurrentContext != nullptr) {
			return true;
		}

		std::string lastFailure;
		std::size_t probed = 0;

		for (const auto& module : CandidateModules()) {
			// A module that does not even export the context getter cannot be
			// an ImGui host; skip it without paying for the whole table.
			if (::GetProcAddress(module.handle, "igGetCurrentContext") == nullptr) {
				continue;
			}

			++probed;

			std::string reason;
			if (TryBindModule(module.handle, module.name, reason)) {
				g_status = "cimgui bound to " + g_host;
				LOG(debug, "ImGuiApi: {}", g_status);
				return true;
			}
			lastFailure = std::move(reason);
		}

		g_status = "no module exports the cimgui API (hosts probed: " + std::to_string(probed) + ")";
		if (!lastFailure.empty()) {
			g_status += "; " + lastFailure;
		}

		// A 2.x framework ships an imgui.dll that exports the C++ API instead;
		// say so explicitly instead of leaving the user with "not found".
		for (const auto& module : CandidateModules()) {
			if (::GetProcAddress(module.handle, "?GetCurrentContext@ImGui@@YAPEAUImGuiContext@@XZ") != nullptr ||
				::GetProcAddress(module.handle, "?Text@ImGui@@YAXPEBDZZ") != nullptr) {
				g_status += "; " + module.name + " exports the C++ ImGui API (menu framework 2.x layout)";
				break;
			}
		}

		if (!g_failureLogged) {
			g_failureLogged = true;
			LOG(err, "ImGuiApi: {}", g_status);
		}
		return false;
	}

	const char* HostName()
	{
		return g_host.empty() ? "<none>" : g_host.c_str();
	}

	std::string Status()
	{
		return g_status;
	}
}
