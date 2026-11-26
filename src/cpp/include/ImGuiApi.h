#pragma once

// ImGui is not linked against this plugin.
//
// The SKSE Menu Framework host owns the ImGui instance that draws the menu.
// Version 3 of the framework (QTR-Modding/SKSE-Menu-Framework-3) compiles the
// ImGui sources straight into SKSEMenuFramework.dll and re-exports a C API:
// cimgui, i.e. every entry point is a plain "ig..." export taking the C
// structs by value, C++ names are not exported at all. Older 2.x builds ship a
// separate imgui.dll instead.
//
// Hard-importing imgui.dll produced "Error 126" on machines that had a
// perfectly working framework, so every entry point is now resolved at
// runtime with GetProcAddress and called through a function pointer table.
//
// The signatures below are copied verbatim from the framework's own plugin
// header (resources/SKSEMenuFramework.h): the C structs are passed BY VALUE and
// each call passes the full argument list, because the default arguments of
// the host build are unknown to us and leaving registers undefined would
// corrupt the call.

#include <imgui.h>

#include "I18n.h"

#include <cstdarg>
#include <cstddef>
#include <string>

namespace SOS::ImGuiApi
{
	struct Api
	{
		// context
		ImGuiContext* (*GetCurrentContext)();
		bool (*IsAnyItemActive)();

		// layout / text
		void (*SameLine)(float, float);
		void (*Spacing)();
		void (*Separator)();
		void (*SeparatorText)(const char*);
		void (*SetNextItemWidth)(float);
		void (*TextV)(const char*, va_list);
		void (*TextUnformatted)(const char*, const char*);
		void (*TextDisabledV)(const char*, va_list);
		void (*TextWrappedV)(const char*, va_list);
		void (*SetTooltipV)(const char*, va_list);

		// widgets
		bool (*Button)(const char*, const ImVec2);
		bool (*SmallButton)(const char*);
		bool (*Checkbox)(const char*, bool*);
		bool (*Selectable)(const char*, bool, ImGuiSelectableFlags, const ImVec2);
		bool (*BeginCombo)(const char*, const char*, ImGuiComboFlags);
		void (*EndCombo)();
		bool (*BeginChild)(const char*, const ImVec2, ImGuiChildFlags, ImGuiWindowFlags);
		void (*EndChild)();
		bool (*TreeNode)(const char*);
		void (*TreePop)();
		bool (*CollapsingHeader)(const char*, ImGuiTreeNodeFlags);
		bool (*SliderInt)(const char*, int*, int, int, const char*, ImGuiSliderFlags);
		bool (*InputInt)(const char*, int*, int, int, ImGuiInputTextFlags);
		bool (*InputText)(const char*, char*, std::size_t, ImGuiInputTextFlags, ImGuiInputTextCallback, void*);
		bool (*InputTextWithHint)(const char*, const char*, char*, std::size_t, ImGuiInputTextFlags, ImGuiInputTextCallback, void*);
		void (*PushIDInt)(int);
		void (*PushIDPtr)(const void*);
		void (*PopID)();
		bool (*IsItemHovered)(ImGuiHoveredFlags);
		bool (*IsMouseDoubleClicked)(ImGuiMouseButton);

		// ---- optional helpers ------------------------------------------------
		//
		// Bound separately from the table above: the framework is free to drop
		// any of these in a future release, and when one is missing only the
		// window plumbing and the keyboard/gamepad navigation of the quick
		// picker degrade, the rest of the menu keeps working.
		struct Extras
		{
			bool (*Begin)(const char*, bool*, ImGuiWindowFlags);
			void (*End)();
			void (*SetNextWindowPos)(const ImVec2, ImGuiCond, const ImVec2);
			void (*SetNextWindowSize)(const ImVec2, ImGuiCond);
			ImGuiViewport* (*GetMainViewport)();
			// Returned as void* on purpose: imgui.h does not declare ImGuiWindow
			// (it lives in imgui_internal.h) and only the null check matters here.
			void* (*GetCurrentWindow)();
			bool (*IsKeyDown)(ImGuiKey);
			bool (*IsKeyPressed)(ImGuiKey, bool);
			void (*GetContentRegionAvail)(ImVec2*);
			void (*CalcTextSize)(ImVec2*, const char*, const char*, bool, float);
			float (*GetScrollY)();
			void (*SetScrollHereY)(float);
			bool (*IsItemActivated)();
			void (*Dummy)(const ImVec2);
			// Window rectangle, used to remember where the quick picker was.
			void (*GetWindowPos)(ImVec2*);
			void (*GetWindowSize)(ImVec2*);
			// Input plumbing, used while a hotkey is being recorded: the host's
			// ImGui instance has to stop queueing the very keys that are being
			// recorded, otherwise a controller button that records a chord also
			// activates whatever the menu has focused.
			ImGuiIO* (*GetIO)();
			void (*ImGuiIO_SetAppAcceptingEvents)(ImGuiIO*, bool);
			void (*ImGuiIO_ClearEventsQueue)(ImGuiIO*);
			void (*ImGuiIO_ClearInputKeys)(ImGuiIO*);
			// Raises the next window above the framework's own windows.
			void (*SetNextWindowFocus)();
			// Greys out and locks the page underneath a modal child: the recorder
			// window owns the input while it is open, so everything below it must
			// look (and be) out of reach.
			void (*BeginDisabled)(bool);
			void (*EndDisabled)();
			// Font query, used to tell whether the language the player picked can
			// actually be drawn: this plugin ships no font and does not second-guess
			// the one the framework loaded, it asks that very font for a glyph.
			ImFont* (*GetFont)();
			const ImFontGlyph* (*FindGlyphNoFallback)(ImFont*, ImWchar);

		};

		// Optional cimgui item-flag helpers. They let the quick picker keep the
		// menu framework's gamepad navigation alive while its own widgets are
		// not reachable through ImGui navigation.
		void (*PushItemFlag)(int, bool);
		void (*PopItemFlag)();

		Extras extras{};
		bool hasExtras{ false };
		// Only the four calls needed to own a window. Everything else is
		// optional and must never stop the quick picker from opening.
		bool hasWindowPlumbing{ false };
		// Kept apart from hasExtras: a missing rectangle only costs the window
		// position memory, everything else keeps working.
		bool hasWindowRect{ false };
		// True when PushItemFlag/PopItemFlag were found. When they are not,
		// the quick picker simply leaves ImGui navigation enabled.
		bool hasItemFlags{ false };
		// True when igGetIO and the three ImGuiIO entry points were found. Only
		// the hotkey recorder uses them; everything else works without them.
		bool hasInputSuppression{ false };
		// True when a window can be raised above the framework's own windows.
		bool hasWindowFocus{ false };
		// True when the page under a modal window can be greyed out and locked.
		bool hasDisabled{ false };
		// True when the font of the host can be inspected. Without it the language
		// choice is left alone instead of being switched off for an unknown font.
		bool hasFontProbe{ false };

	};

	// The process-wide binding table. Every pointer is null until Bind() ran.
	Api& Get();

	// Resolves the cimgui entry points from the loaded menu framework (or from
	// whichever module exports them). Cheap when it already succeeded. Returns
	// true only when every entry point was found.
	bool Bind();

	// Bound module file name ("SKSEMenuFramework.dll", ...).
	const char* HostName();

	// Human readable status line for the log.
	std::string Status();
}

// Thin wrappers so the panel code reads exactly like normal ImGui calls while
// actually dispatching through the runtime table. Each wrapper supplies the
// full argument list.
namespace SOS::IG
{
	inline void Text(const char* a_fmt, ...)
	{
		va_list args;
		va_start(args, a_fmt);
		if (const auto fn = ImGuiApi::Get().TextV) { fn(I18n::Tr(a_fmt), args); }
		va_end(args);
	}

	inline void TextDisabled(const char* a_fmt, ...)
	{
		va_list args;
		va_start(args, a_fmt);
		if (const auto fn = ImGuiApi::Get().TextDisabledV) { fn(I18n::Tr(a_fmt), args); }
		va_end(args);
	}

	inline void TextWrapped(const char* a_fmt, ...)
	{
		va_list args;
		va_start(args, a_fmt);
		if (const auto fn = ImGuiApi::Get().TextWrappedV) { fn(I18n::Tr(a_fmt), args); }
		va_end(args);
	}

	// Whether the font the host is drawing the menu with can show Chinese:
	// 1 = yes, 0 = no, -1 = not known yet (no font query in this host, or no font
	// is current). Only called while the menu is open, because igGetFont reads the
	// ImGui context of the host.
	inline int ChineseGlyphSupport()
	{
		const auto& api = ImGuiApi::Get();
		if (!api.hasFontProbe) {
			return -1;
		}
		const auto font = api.extras.GetFont ? api.extras.GetFont() : nullptr;
		if (!font) {
			return -1;
		}
		if (api.extras.FindGlyphNoFallback(font, static_cast<ImWchar>(0x4E2D))) {
			return 1;
		}
		if (api.extras.FindGlyphNoFallback(font, static_cast<ImWchar>(0x6587))) {
			return 1;
		}
		return 0;
	}

	inline void SetTooltip(const char* a_fmt, ...)
	{
		va_list args;
		va_start(args, a_fmt);
		if (const auto fn = ImGuiApi::Get().SetTooltipV) { fn(I18n::Tr(a_fmt), args); }
		va_end(args);
	}

	inline void TextUnformatted(const char* a_text)
	{
		if (const auto fn = ImGuiApi::Get().TextUnformatted) { fn(I18n::Tr(a_text), nullptr); }
	}

	inline void SameLine() { if (const auto fn = ImGuiApi::Get().SameLine) { fn(0.0f, -1.0f); } }
	inline void SameLine(float a_offset) { if (const auto fn = ImGuiApi::Get().SameLine) { fn(a_offset, -1.0f); } }
	inline void Spacing() { if (const auto fn = ImGuiApi::Get().Spacing) { fn(); } }
	inline void Separator() { if (const auto fn = ImGuiApi::Get().Separator) { fn(); } }
	inline void SeparatorText(const char* a_label) { if (const auto fn = ImGuiApi::Get().SeparatorText) { fn(I18n::TrId(a_label)); } }
	inline void SetNextItemWidth(float a_width) { if (const auto fn = ImGuiApi::Get().SetNextItemWidth) { fn(a_width); } }

	inline bool Button(const char* a_label)
	{
		const auto fn = ImGuiApi::Get().Button;
		return fn ? fn(I18n::TrId(a_label), ImVec2(0.0f, 0.0f)) : false;
	}

	inline bool SmallButton(const char* a_label)
	{
		const auto fn = ImGuiApi::Get().SmallButton;
		return fn ? fn(I18n::TrId(a_label)) : false;
	}

	inline bool Checkbox(const char* a_label, bool* a_value)
	{
		const auto fn = ImGuiApi::Get().Checkbox;
		return fn ? fn(I18n::TrId(a_label), a_value) : false;
	}

	inline bool Selectable(const char* a_label, bool a_selected = false, int a_flags = 0)
	{
		const auto fn = ImGuiApi::Get().Selectable;
		return fn ? fn(I18n::TrId(a_label), a_selected, static_cast<ImGuiSelectableFlags>(a_flags), ImVec2(0.0f, 0.0f)) : false;
	}

	inline bool BeginCombo(const char* a_label, const char* a_preview)
	{
		const auto fn = ImGuiApi::Get().BeginCombo;
		return fn ? fn(I18n::TrId(a_label), I18n::Tr(a_preview), ImGuiComboFlags_None) : false;
	}

	inline void EndCombo() { if (const auto fn = ImGuiApi::Get().EndCombo) { fn(); } }

	inline bool BeginChild(const char* a_id, const ImVec2& a_size, bool a_border)
	{
		const auto fn = ImGuiApi::Get().BeginChild;
		return fn ? fn(a_id, a_size, a_border ? ImGuiChildFlags_Border : ImGuiChildFlags_None, ImGuiWindowFlags_None) : false;
	}

	inline void EndChild() { if (const auto fn = ImGuiApi::Get().EndChild) { fn(); } }

	inline bool TreeNode(const char* a_label)
	{
		const auto fn = ImGuiApi::Get().TreeNode;
		return fn ? fn(I18n::TrId(a_label)) : false;
	}

	inline void TreePop() { if (const auto fn = ImGuiApi::Get().TreePop) { fn(); } }

	inline bool CollapsingHeader(const char* a_label)
	{
		const auto fn = ImGuiApi::Get().CollapsingHeader;
		return fn ? fn(I18n::TrId(a_label), ImGuiTreeNodeFlags_None) : false;
	}

	inline bool SliderInt(const char* a_label, int* a_value, int a_min, int a_max)
	{
		const auto fn = ImGuiApi::Get().SliderInt;
		return fn ? fn(I18n::TrId(a_label), a_value, a_min, a_max, "%d", ImGuiSliderFlags_None) : false;
	}

	inline bool InputInt(const char* a_label, int* a_value, int a_step)
	{
		const auto fn = ImGuiApi::Get().InputInt;
		return fn ? fn(I18n::TrId(a_label), a_value, a_step, 100, ImGuiInputTextFlags_None) : false;
	}

	inline bool InputText(const char* a_label, char* a_buffer, std::size_t a_size)
	{
		const auto fn = ImGuiApi::Get().InputText;
		return fn ? fn(I18n::TrId(a_label), a_buffer, a_size, ImGuiInputTextFlags_None, nullptr, nullptr) : false;
	}

	inline bool InputTextWithHint(const char* a_label, const char* a_hint, char* a_buffer, std::size_t a_size)
	{
		const auto fn = ImGuiApi::Get().InputTextWithHint;
		return fn ? fn(I18n::TrId(a_label), I18n::Tr(a_hint), a_buffer, a_size, ImGuiInputTextFlags_None, nullptr, nullptr) : false;
	}

	inline void PushID(int a_id) { if (const auto fn = ImGuiApi::Get().PushIDInt) { fn(a_id); } }
	inline void PushID(const void* a_id) { if (const auto fn = ImGuiApi::Get().PushIDPtr) { fn(a_id); } }
	inline void PopID() { if (const auto fn = ImGuiApi::Get().PopID) { fn(); } }

	inline bool IsItemHovered()
	{
		const auto fn = ImGuiApi::Get().IsItemHovered;
		return fn ? fn(ImGuiHoveredFlags_None) : false;
	}

	inline bool IsMouseDoubleClicked(int a_button)
	{
		const auto fn = ImGuiApi::Get().IsMouseDoubleClicked;
		return fn ? fn(static_cast<ImGuiMouseButton>(a_button)) : false;
	}

	// ---- optional helpers ----------------------------------------------------

	inline bool HasExtras() { return ImGuiApi::Get().hasExtras; }

	// True when the framework exports everything needed to open a window.
	inline bool WindowPlumbingReady() { return ImGuiApi::Get().hasWindowPlumbing; }

	// True only when the framework already opened a window for us. The SKSE
	// Menu Framework calls every registered render function without an
	// ImGui::Begin/End pair, so such a callback has to provide its own window.
	inline bool WindowIsOpen()
	{
		const auto& api = ImGuiApi::Get();
		return api.extras.GetCurrentWindow && api.extras.GetCurrentWindow() != nullptr;
	}

	inline bool BeginWindow(const char* a_title, bool* a_open, ImGuiWindowFlags a_flags = 0)
	{
		const auto& api = ImGuiApi::Get();
		return api.extras.Begin ? api.extras.Begin(a_title, a_open, a_flags) : false;
	}

	inline void EndWindow()
	{
		const auto& api = ImGuiApi::Get();
		if (api.extras.End) { api.extras.End(); }
	}

	// Position and size of the window that is currently being drawn. Both have
	// to be read between BeginWindow() and EndWindow().
	inline bool GetWindowPos(ImVec2* a_out)
	{
		const auto& api = ImGuiApi::Get();
		if (api.hasWindowRect && api.extras.GetWindowPos) {
			api.extras.GetWindowPos(a_out);
			return true;
		}
		return false;
	}

	inline bool GetWindowSize(ImVec2* a_out)
	{
		const auto& api = ImGuiApi::Get();
		if (api.hasWindowRect && api.extras.GetWindowSize) {
			api.extras.GetWindowSize(a_out);
			return true;
		}
		return false;
	}

	inline void SetNextWindowPos(const ImVec2& a_pos, ImGuiCond a_cond, const ImVec2& a_pivot)
	{
		const auto& api = ImGuiApi::Get();
		if (api.extras.SetNextWindowPos) { api.extras.SetNextWindowPos(a_pos, a_cond, a_pivot); }
	}

	inline void SetNextWindowSize(const ImVec2& a_size, ImGuiCond a_cond)
	{
		const auto& api = ImGuiApi::Get();
		if (api.extras.SetNextWindowSize) { api.extras.SetNextWindowSize(a_size, a_cond); }
	}

	// Raises the window opened by the next BeginWindow() above the windows the
	// framework itself creates later in the same frame. Without it the hotkey
	// recorder ends up underneath the settings page.
	inline void SetNextWindowFocus()
	{
		const auto& api = ImGuiApi::Get();
		if (api.hasWindowFocus && api.extras.SetNextWindowFocus) { api.extras.SetNextWindowFocus(); }
	}

	// True when the host can grey out (and lock) everything drawn inside the
	// disabled block. Without it the recorder window still works, the page
	// underneath simply stays clickable.
	inline bool DisabledReady()
	{
		return ImGuiApi::Get().hasDisabled;
	}

	inline void BeginDisabled(bool a_disabled)
	{
		const auto& api = ImGuiApi::Get();
		if (api.hasDisabled && api.extras.BeginDisabled) { api.extras.BeginDisabled(a_disabled); }
	}

	inline void EndDisabled()
	{
		const auto& api = ImGuiApi::Get();
		if (api.hasDisabled && api.extras.EndDisabled) { api.extras.EndDisabled(); }
	}

	// True when the host's ImGui can be told to stop accepting input.
	inline bool InputSuppressionReady()

	{
		return ImGuiApi::Get().hasInputSuppression;
	}

	// Stops (or resumes) input delivery for the host's ImGui instance. Called
	// once per rendered frame while a hotkey is being recorded and therefore
	// idempotent: it only touches the host when the state changed, except while
	// suppressing, where the queue is cleared again on every call. Anything
	// still queued would otherwise be picked up by the next frame.
	inline void SetMenuInputSuppressed(bool a_suppress)
	{
		const auto& api = ImGuiApi::Get();
		if (!api.hasInputSuppression) {
			return;
		}
		static bool applied = false;
		static bool last = false;
		if (applied && last == a_suppress && !a_suppress) {
			return;
		}
		if (auto* io = api.extras.GetIO()) {
			if (a_suppress) {
				api.extras.ImGuiIO_ClearEventsQueue(io);
				api.extras.ImGuiIO_ClearInputKeys(io);
			}
			api.extras.ImGuiIO_SetAppAcceptingEvents(io, !a_suppress);
			applied = true;
			last = a_suppress;
		}
	}

	inline ImVec2 MainViewportCenter()
	{
		const auto& api = ImGuiApi::Get();
		if (api.extras.GetMainViewport) {
			const auto viewport = api.extras.GetMainViewport();
			if (viewport) {
				return ImVec2{ viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f };
			}
		}
		return ImVec2{ 0.0f, 0.0f };
	}

	inline ImVec2 MainViewportSize()
	{
		const auto& api = ImGuiApi::Get();
		if (api.extras.GetMainViewport) {
			const auto viewport = api.extras.GetMainViewport();
			if (viewport) {
				return viewport->Size;
			}
		}
		return ImVec2{ 1280.0f, 720.0f };
	}

	inline bool IsKeyDown(ImGuiKey a_key)
	{
		const auto& api = ImGuiApi::Get();
		return api.extras.IsKeyDown ? api.extras.IsKeyDown(a_key) : false;
	}

	inline bool IsKeyPressed(ImGuiKey a_key, bool a_repeat = false)
	{
		const auto& api = ImGuiApi::Get();
		return api.extras.IsKeyPressed ? api.extras.IsKeyPressed(a_key, a_repeat) : false;
	}

	inline ImVec2 ContentRegionAvail()
	{
		ImVec2 size{ 0.0f, 0.0f };
		const auto& api = ImGuiApi::Get();
		if (api.extras.GetContentRegionAvail) { api.extras.GetContentRegionAvail(&size); }
		return size;
	}

	inline ImVec2 CalcTextSize(const char* a_text)
	{
		ImVec2 size{ 0.0f, 0.0f };
		const auto& api = ImGuiApi::Get();
		if (api.extras.CalcTextSize && a_text) { api.extras.CalcTextSize(&size, a_text, nullptr, false, -1.0f); }
		return size;
	}

	inline float GetScrollY()
	{
		const auto& api = ImGuiApi::Get();
		return api.extras.GetScrollY ? api.extras.GetScrollY() : 0.0f;
	}

	inline void SetScrollHereY(float a_centerY)
	{
		const auto& api = ImGuiApi::Get();
		if (api.extras.SetScrollHereY) { api.extras.SetScrollHereY(a_centerY); }
	}

	inline bool IsItemActivated()
	{
		const auto& api = ImGuiApi::Get();
		return api.extras.IsItemActivated ? api.extras.IsItemActivated() : false;
	}

	inline void Dummy(const ImVec2& a_size)
	{
		const auto& api = ImGuiApi::Get();
		if (api.extras.Dummy) { api.extras.Dummy(a_size); }
	}

	// Push ImGuiItemFlags_NoNav (1 << 3 in ImGui 1.90.x) so the quick picker's
	// widgets never become nav targets. The window itself still counts as a
	// nav window, which keeps io.NavActive true for the framework's menu.
	inline bool PushItemFlagNoNav()
	{
		const auto& api = ImGuiApi::Get();
		if (api.hasItemFlags && api.PushItemFlag) {
			api.PushItemFlag(1 << 3, true);
			return true;
		}
		return false;
	}

	inline void PopItemFlag()
	{
		const auto& api = ImGuiApi::Get();
		if (api.hasItemFlags && api.PopItemFlag) { api.PopItemFlag(); }
	}

	inline bool BeginChild(const char* a_id, const ImVec2& a_size, bool a_border, ImGuiWindowFlags a_flags)
	{
		const auto fn = ImGuiApi::Get().BeginChild;
		return fn ? fn(a_id, a_size, a_border ? ImGuiChildFlags_Border : ImGuiChildFlags_None, a_flags) : false;
	}

	inline bool Selectable(const char* a_label, bool a_selected, int a_flags, const ImVec2& a_size)
	{
		const auto fn = ImGuiApi::Get().Selectable;
		return fn ? fn(I18n::TrId(a_label), a_selected, static_cast<ImGuiSelectableFlags>(a_flags), a_size) : false;
	}
}
