#pragma once

#include "core/Types.h"

#include <cstddef>
#include <optional>

namespace RE {
    namespace BSScript {
        class IVirtualMachine;
    }
}

namespace RE {
    class Actor;
}

namespace SOS {
    class OutfitService;
}

namespace OutfitSystem {
    bool RegisterPapyrus(RE::BSScript::IVirtualMachine* registry);
    bool RegisterEvents();

    // Shared by the state watcher and the UI: classifies the actor's current
    // location/weather/combat state. Returns nullopt when the actor has no
    // outfit configured for that state.
    std::optional<StateType> identifyStateType(const SOS::OutfitService& a_service, RE::Actor* a_actor);

    // Rebuilds the actor's biped. Must be called without the outfit lock held.
    void refreshArmorFor(RE::Actor* a_actor);

    // Rebuilds every actor that is currently wearing \a a_outfit. An edit or a
    // slot policy change only shows up once that actor walks through a skinning
    // pass, and a slot that leaves the display set has to be cleared, otherwise
    // its clone stays on the body. Must be called without the outfit lock held.
    std::size_t refreshArmorForOutfitUsers(const char* a_outfit);

    // Applies the outfit configured for the actor's current state, if any.
    void setOutfitUsingState(SOS::OutfitService& a_service, RE::Actor* a_actor);

    // Assigns a named outfit the way the state based switching assigns its own:
    // the store is only held for the assignment, the caller refreshes the biped
    // afterwards with refreshArmorFor (see AutoSwitch::Evaluate).
    // Returns false when the outfit does not exist; nothing changes then.
    bool setOutfitByName(SOS::OutfitService& a_service, RE::Actor* a_actor, const char* a_name);

}
