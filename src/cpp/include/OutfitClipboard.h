#pragma once

#include <string>

namespace SOS::OutfitClipboard
{
	enum class Result
	{
		Ok,
		ClipboardEmpty,     // nothing (or no text) on the clipboard
		ClipboardOpenFail,  // another process holds the clipboard
		InvalidJson,
		NoOutfit,           // valid JSON, but no outfit object in it
		NameTooLong,
		CreateFailed,       // the store refused the name (should not happen)
	};

	// Serialises one outfit - name, favourite flag, armour pieces, blanket slot
	// policy and every per slot override - as the same JSON object the legacy
	// OutfitSystemData.json uses, and puts it on the Windows clipboard.
	bool Copy(const std::string& a_outfitName);

	// Reads one outfit object from the clipboard (a bare object, or an object
	// with a non-empty "outfits" array whose first entry is used), recreates it
	// under a free name (name, then name_copy, name_copy2, ...) and reports the
	// name that was actually created.
	Result Paste(std::string& a_createdName);
}
