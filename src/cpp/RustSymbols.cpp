//
// Created by m on 10/23/2022.
//

#include "bindings.h"

namespace RE {
    [[maybe_unused]] TESObjectARMO* LookupARMOFormID(FormID id) {
        return skyrim_cast<RE::TESObjectARMO*>(RE::TESForm::LookupByID(id));
    }
    [[maybe_unused]] Actor* LookupActorFormID(FormID id) {
        return skyrim_cast<RE::Actor*>(RE::TESForm::LookupByID(id));
    }
    [[maybe_unused]] void instantiate() {
        RE::PlayerCharacter::GetSingleton();
        REL::Module::reset();
        auto a = ((RE::TESForm*) nullptr)->GetLocalFormID();
    }
    FormID TESDataHandler_LookupFormIDRawC(TESDataHandler* dh, FormID raw_form_id, const char* mod_name) {
        return dh->LookupFormIDRaw(raw_form_id, std::string_view(mod_name));
    }
    FormID TESDataHandler_LookupFormIDC(TESDataHandler* dh, FormID local_form_id, const char* mod_name) {
        return dh->LookupFormID(local_form_id, std::string_view(mod_name));
    }
    const char* BSFixedString_CStr(const BSFixedString* string) {
        return string->c_str();
    }
}

namespace OutfitSystem {
    namespace ArmorFormSearchUtils {
        std::unique_ptr<std::vector<TESObjectARMOPtr>> RustSearch(rust::Str query);
    }
    void refreshArmorForAllConfiguredActors();
}

std::unique_ptr<std::vector<TESObjectARMOPtr>> ArmorSearch(rust::Str query) {
    return OutfitSystem::ArmorFormSearchUtils::RustSearch(query);
}

void CreateRefreshAllArmorsTask() {
    auto task_intfc = SKSE::GetTaskInterface();
    if (!task_intfc) {
        return;
    }
    task_intfc->AddTask([]() {
        OutfitSystem::refreshArmorForAllConfiguredActors();
    });
}
