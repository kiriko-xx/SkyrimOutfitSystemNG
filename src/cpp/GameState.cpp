#include "GameState.h"

#include <atomic>

namespace SOS::GameState
{
	namespace
	{
		// Starts as "loading": the plugin is loaded while the game sits on the
		// main menu, long before any world exists.
		std::atomic<bool> g_loading{ true };
	}

	bool IsLoading()
	{
		return g_loading.load(std::memory_order_acquire);
	}

	void SetLoading(bool a_loading)
	{
		g_loading.store(a_loading, std::memory_order_release);
	}
}// namespace SOS::GameState
