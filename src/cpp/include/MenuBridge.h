#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace RE
{
	class InputEvent;
}

// Resolves the SKSE Menu Framework DLL at runtime instead of import-linking
// it. Two reasons:
//   1. a missing framework must not stop the plugin from loading (that was the
//      "Error 126" report),
//   2. the framework may be installed anywhere in the virtual file system that
//      the mod manager builds, so we search instead of relying on the loader.
//
// The framework's own header is intentionally not included: nothing here is
// link-time bound, and the two entry points are resolved with GetProcAddress.
namespace SOS::MenuBridge
{
	typedef void(__stdcall* RenderFunction)();

	// Mirrors SKSEMenuFramework::Model::WindowInterface. 3.x appends
	// BlockUserInput; 2.x does not, so the second field is only written when
	// the framework reports a 3.x API in AddWindow().
	struct WindowInterface
	{
		std::atomic<bool> IsOpen{ false };
		std::atomic<bool> BlockUserInput{ true };
	};

	// SMF 3.x input callback: return true when the event was consumed.
	typedef bool(__stdcall* InputEventCallback)(RE::InputEvent*);

	// SMF 3.x menu lifecycle callback (Model::EventType): 1 = opened, 2 =
	// closed, 3 = before a render pass, 4 = after one. The open and close
	// notifications are used: the hotkeys are muted while the menu owns the
	// input, and a hotkey recording has to end when the menu goes away.
	typedef void(__stdcall* EventCallback)(int a_eventType);
	inline constexpr int kEventMenuOpened = 1;
	inline constexpr int kEventMenuClosed = 2;

	// Finds (or loads) SKSEMenuFramework.dll and resolves its entry points.
	// Safe to call repeatedly; returns false when the framework is unusable.
	bool EnsureLoaded();

	// True once EnsureLoaded() succeeded.
	bool Available();

	// Entry points. Both are no-ops that return null when the framework is
	// unavailable, so callers do not need to check first.
	WindowInterface* AddWindow(RenderFunction a_render);
	void			 AddSectionItem(const char* a_path, RenderFunction a_render);

	// Registers a raw input callback with the framework. This is the reliable
	// path for the gamepad: the framework translates keyboard/mouse into ImGui
	// itself, but gamepad translation depends on its menu window being the
	// active one. Returns false when the framework has no input-event export.
	bool RegisterInputEvent(InputEventCallback a_callback);

	// Registers a menu lifecycle callback. Returns false when the framework has
	// no such export (2.x) or refused the callback.
	bool RegisterEventCallback(EventCallback a_callback);

	// True once a menu lifecycle callback is registered. A hotkey recording may only
	// withhold the host input when this is true: the close notification is the only
	// reliable way to give it back if the menu goes away mid recording.
	bool EventCallbackAvailable();

	// True while any window the framework owns blocks the user input, which is
	// what a menu being open looks like from the outside. False when the export is
	// missing, so a host without it only loses this extra guard.
	bool AnyBlockingWindowOpened();
	float FrameworkVersion();

	// Directory that contains this plugin DLL.
	std::wstring ModuleDirectory();

	// True while an ImGui text field owns the keyboard. False when the menu is
	// unavailable, so hotkeys keep working without a framework.
	bool TextInputActive();
}
