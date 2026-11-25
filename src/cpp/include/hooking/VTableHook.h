#pragma once

#include <cstddef>
#include <cstdint>

namespace Hooking
{
	// Replaces one slot of a virtual table and hands the previous entry back.
	//
	// Some engine objects are created by the game itself and are only reachable
	// through their virtual table: the address library knows where the table is, not
	// where the object is. Taking over one slot of that table is the way to run code
	// on every poll of that device without patching any machine code, which keeps the
	// hook working when a build recompiles the functions around it.
	//
	// Every step is checked, and a failed install returns nullptr instead of leaving a
	// broken table behind:
	//   * an unknown address library id (resolving it would end the process);
	//   * a table or slot outside committed, readable memory;
	//   * an empty slot;
	//   * another plugin that replaced the slot between the read and the write - the
	//     exchange is a compare-and-swap, so the other plugin keeps its hook and this
	//     one stays out instead of dropping it silently.
	//
	// a_original receives the entry that was there, which the replacement has to call
	// itself: the game still needs the device to update its state.
	void* InstallVFunc(const char* a_name, std::uintptr_t a_table, std::size_t a_slot, void* a_hook, void** a_original);
}
