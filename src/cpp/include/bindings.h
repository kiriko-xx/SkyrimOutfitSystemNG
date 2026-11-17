//
// Stand-in for the "bindings.h" that the cxx bridge generated from src/rust.
//
// The whole data model used to live in Rust and was reached through a cxx
// bridge. This port implements the very same surface in plain C++ (src/core)
// so that the Papyrus layer and the hooks stay identical to the original mod.

#ifndef SKYRIMOUTFITSYSTEMSE_INCLUDE_BINDINGS_H
#define SKYRIMOUTFITSYSTEMSE_INCLUDE_BINDINGS_H

#include "core/Outfit.h"
#include "core/OutfitService.h"
#include "core/Policy.h"
#include "core/Settings.h"
#include "core/Types.h"

#include <cstdint>
#include <string>
#include <vector>

// Free functions that used to be provided by the Rust half of the bridge.
bool is_form_id_permitted(std::uint32_t a_formID);
std::vector<MetadataC> list_available_policies_c(bool a_allowAdvanced);
rust::String translation_key_c(const rust::String& a_code);
std::int8_t nat_ord_case_insensitive_c(const rust::String& a_lhs, const rust::String& a_rhs);
bool settings_extra_logging_enabled();

// Runs the "refresh every configured actor" pass on the game thread.
void CreateRefreshAllArmorsTask();

#endif//SKYRIMOUTFITSYSTEMSE_INCLUDE_BINDINGS_H
