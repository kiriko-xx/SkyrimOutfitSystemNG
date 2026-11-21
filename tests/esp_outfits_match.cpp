#include "core/EspOutfitMatch.h"
#include "core/EspOutfitNames.h"

#include <cstdlib>
#include <iostream>

namespace Match = SOS::EspOutfits::Match;
namespace Names = SOS::EspOutfits::Names;
using Match::IndexList;
using Match::Piece;
constexpr auto hair = 1u << 1, top = 1u << 2, shoes = 1u << 7, skirt = 1u << 18;
#define CHECK(...) do { if (!(__VA_ARGS__)) { std::cerr << "Failure at line " << __LINE__ << '\n'; std::exit(1); } } while (false)

Piece Item(std::string_view name, std::uint32_t mask) { return { Names::ComparisonKey(name), mask }; }

int main()
{
    CHECK(Names::ComparisonKey("星夜礼服＿裙子Ａ") == U"星夜礼服裙子a");
    CHECK(Names::ComparisonKey("Étoile　haut") == Names::ComparisonKey("e\u0301toile-haut"));
    CHECK(Names::ComparisonKey("Ночь  Плащ") == U"ночьплащ");
    CHECK(Names::ComparisonKey("𠮷") == U"𠮷");
    CHECK(Names::ComparisonKey("\xC3\x28").empty());
    CHECK(Names::OriginalPrefix("星夜礼服-上衣A", U"星夜礼服") == "星夜礼服");
    CHECK(Names::SlotNumbers(top | shoes | (1u << 31)) == "32, 37, 61");
    CHECK(Names::PresetName("Wardrobe", "星夜礼服", 1, 1, 0) == "Wardrobe");
    CHECK(Names::PresetName("Wardrobe", "星夜礼服", 1, 2, 0) == "Wardrobe_1");
    CHECK(Names::PresetName("Wardrobe", "星夜礼服", 1, 2, 1) == "Wardrobe_2");
    CHECK(Names::PresetName("Wardrobe", "星夜礼服", 2, 1, 0) == "Wardrobe_星夜礼服");
    CHECK(Names::PresetName("Wardrobe", "星夜礼服", 2, 2, 0) == "Wardrobe_星夜礼服_1");
    CHECK(Names::PresetName("Wardrobe", "星夜礼服", 2, 2, 1) == "Wardrobe_星夜礼服_2");
    CHECK(Names::PresetName("Wardrobe", "", 2, 1, 0).empty());

    std::vector<Piece> items{ Item("星夜礼服_假发A", hair), Item("星夜礼服-上衣A", top), Item("星夜礼服 上衣Ｂ", top),
        Item("星夜礼服裙子A", skirt), Item("星夜礼服裙子B", skirt), Item("星夜礼服鞋子A", shoes) };
    auto groups = Match::Plan(items);
    CHECK(groups.size() == 1 && groups[0].prefix == U"星夜礼服" && !groups[0].issue);
    CHECK(groups[0].variants == std::vector<IndexList>({ {0, 1, 3, 5}, {0, 2, 4, 5} }));
    items.push_back(Item("星夜晚礼服上衣A", top));
    items.push_back(Item("星夜晚礼服裙子A", skirt));
    items.push_back(Item("星夜晚礼服假发A", hair));
    groups = Match::Plan(items);
    CHECK(groups.size() == 2);
    CHECK(groups[0].prefix == U"星夜晚礼服" && groups[0].variants.size() == 1);
    CHECK(groups[1].prefix == U"星夜礼服" && groups[1].variants.size() == 2);
    items.push_back(Item("星夜项链", 1u << 5));
    groups = Match::Plan(items);
    CHECK(groups[0].members.size() + groups[1].members.size() == 9);

    items = { Item("星夜礼服上衣A", top), Item("星夜礼服上衣B", top),
        Item("星夜礼服裙子红色", skirt), Item("星夜礼服裙子黑色", skirt) };
    groups = Match::Plan(items);
    CHECK(groups.size() == 1 && groups[0].variants.empty() && std::string_view(groups[0].issue) == "unmatched-difference");
    groups = Match::Plan(items, {{U"红色", U"a"}, {U"黑色", U"b"}});
    CHECK(groups[0].variants == std::vector<IndexList>({{0, 2}, {1, 3}}));
    groups = Match::Plan(items, {{U"红色", U"a"}, {U"黑色", U"a"}});
    CHECK(groups[0].variants.empty() && std::string_view(groups[0].issue) == "non-unique-difference");
    items[3] = items[2];
    groups = Match::Plan(items);
    CHECK(groups[0].variants.empty());
    items = { Item("Orion Top A", top), Item("Orion Top B", top), Item("Orion Skirt A", skirt), Item("Orion Skirt C", skirt) };
    CHECK(Match::Plan(items)[0].variants.empty());
    items = { Item("Orion Top A", top), Item("Orion Top B", top), Item("Orion Shoes", shoes) };
    CHECK(Match::Plan(items)[0].variants == std::vector<IndexList>({{0, 2}, {1, 2}}));
    items = { Item("Orion Top A", top | skirt), Item("Orion Top B", top | skirt), Item("Orion Boots", shoes) };
    CHECK(Match::Plan(items)[0].variants.size() == 2);
    items.push_back(Item("Orion Skirt", skirt));
    groups = Match::Plan(items);
    CHECK(groups[0].variants.empty() && std::string_view(groups[0].issue) == "overlapping-slot-masks");
    CHECK(Match::Plan({Item("Orion Top", top | skirt), Item("Orion Skirt", skirt)}).empty());
    CHECK(Match::Plan({Item("Orion Dress", top | skirt)})[0].variants == std::vector<IndexList>({{0}}));
    CHECK(Match::Plan({Item("星夜礼服上衣A", top), Item("Dawn Skirt", skirt)}).empty());
    CHECK(Match::Plan({Item("Dark Top", top), Item("Dawn Skirt", skirt)}).empty());
    CHECK(Match::Plan({Item("Orion Night Top", top), Item("Orion Night Skirt", skirt), Item("Orion Shoes", shoes)})[0].variants.empty());
    items = { Item("Orion Shoes", shoes) };
    for (std::size_t i = 0; i <= Match::kMaxVariants; ++i) { items.push_back(Item("Orion Top " + std::to_string(i), top)); }
    groups = Match::Plan(items);
    CHECK(groups[0].variants.empty() && std::string_view(groups[0].issue) == "variant-limit");
    std::cout << "ESP_OUTFITS_MATCH_OK: Unicode, family splits, ordered variants, explicit aliases, ambiguity and multi-slot invariants\n";
}
