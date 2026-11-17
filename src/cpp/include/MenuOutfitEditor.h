#pragma once

#include <string>

#include <RE/Skyrim.h>

// The outfit editor that mirrors the old MCM "outfit list -> editor" page:
// adding and removing armour, the body slot listing and the per-slot policies.
//
// The SKSE Menu Framework only draws a flat list of section items, so the
// editor lives next to the browser (right hand column of the Outfits page)
// instead of being a page of its own.
namespace SOS::Menu::Editor
{
	// Draws the editor for one outfit. a_target is the actor the "Wear" and
	// "Take off" buttons act on (0 means the player). a_onClose is invoked by
	// the "Back" button so the caller can drop its selection.
	void Render(const std::string& a_outfit, RE::FormID a_target, void (*a_onClose)());

	// Drops the cached armour search results; call it when the outfit set or
	// the installed plugins changed.
	void Invalidate();
}
