#pragma once

#include <string>

#include <Windows.h>

namespace SOS::Compat
{
	// Full path of a loaded module (empty when it is not in the process).
	std::string ModulePath(HMODULE a_module);

	// "major.minor.build.revision" of a loaded module, empty if it has no
	// version resource.
	std::string ModuleVersion(HMODULE a_module);
}
