#include "SMP.h"

#include "core/Settings.h"

#include <RE/F/FunctionArguments.h>
#include <RE/I/IVirtualMachine.h>
#include <RE/V/VirtualMachine.h>
#include <SKSE/SKSE.h>

namespace SOS::SMP
{
	namespace
	{
		bool g_missingLogged{ false };

		RE::BSScript::Internal::VirtualMachine* GetVM()
		{
			return RE::BSScript::Internal::VirtualMachine::GetSingleton();
		}
	}

	bool IsAvailable()
	{
		auto* vm = GetVM();
		return vm && vm->TypeIsValid("DynamicHDT");
	}

	void ResetActor(RE::Actor* a_actor)
	{
		if (!a_actor || !Settings::Get().autoResetSmp) {
			return;
		}
		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks) {
			return;
		}

		RE::ActorHandle handle(a_actor);
		tasks->AddTask([handle]() {
			auto actorPtr = handle.get();
			RE::Actor* actor = actorPtr.get();
			if (!actor || !actor->Is3DLoaded()) {
				return;
			}

			auto* vm = GetVM();
			if (!vm || !vm->TypeIsValid("DynamicHDT")) {
				if (!g_missingLogged) {
					g_missingLogged = true;
					SKSE::log::debug("SMP: Faster HDT-SMP (DynamicHDT) is not installed; the physics reset is skipped.");
				}
				return;
			}

			auto* args = RE::MakeFunctionArguments(static_cast<RE::Actor*>(actor), false);
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> result;
			const bool ok = vm->DispatchStaticCall("DynamicHDT", "ResetPhysics", args, result);
			delete args;
			if (!ok) {
				SKSE::log::debug("SMP: DynamicHDT.ResetPhysics could not be dispatched for {:08X}.", actor->GetFormID());
			}
		});
	}
}
