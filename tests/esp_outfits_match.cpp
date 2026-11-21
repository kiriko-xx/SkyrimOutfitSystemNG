#include "core/EspOutfitMatch.h"
#include "core/EspOutfitNames.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace Match = SOS::EspOutfits::Match;
namespace Names = SOS::EspOutfits::Names;
using Match::IndexList;
using Match::Piece;

constexpr std::uint32_t hair = 1u << 1, top = 1u << 2, shoes = 1u << 7, skirt = 1u << 18;
constexpr std::uint32_t fx = 1u << 31, amulet = 1u << 5, panty = 1u << 16, bag = 1u << 17;

#define CHECK(...) do { if (!(__VA_ARGS__)) { std::cerr << "Failure at line " << __LINE__ << ': ' << #__VA_ARGS__ << '\n'; std::exit(1); } } while (false)

Piece Item(std::string_view name, std::uint32_t mask) { return { Names::ComparisonKey(name), mask }; }

// Reason recorded for one piece of a family, or nullptr when it was placed.
static const char* ReasonOf(const Match::Group& group, std::size_t index)
{
    for (const auto& [piece, why] : group.pending) {
        if (piece == index) {
            return why;
        }
    }
    return nullptr;
}

// Every preset of a style has to fill every slot that style offers; this holds for
// every case below, so it is checked everywhere instead of case by case.
static void CheckComplete(const std::vector<Match::Group>& groups, const std::vector<Piece>& pieces, int line)
{
    for (const auto& group : groups) {
        for (const auto& variant : group.variants) {
            std::uint32_t mask = 0;
            for (const auto i : variant) {
                mask |= Match::BodySlots(pieces[i].mask);
            }
            if ((group.coverage & ~mask) != 0) {
                std::cerr << "Failure at line " << line << ": variant leaves a slot of its style empty\n";
                std::exit(1);
            }
        }
    }
}

#define CHECK_COMPLETE(groups, pieces) CheckComplete(groups, pieces, __LINE__)

int main()
{
    // ---- names -----------------------------------------------------------
    CHECK(Names::ComparisonKey("星夜礼服＿裙子Ａ") == U"星夜礼服裙子a");
    CHECK(Names::ComparisonKey("Étoile　haut") == Names::ComparisonKey("e\u0301toile-haut"));
    CHECK(Names::ComparisonKey("Ночь  Плащ") == U"ночьплащ");
    CHECK(Names::ComparisonKey("𠮷") == U"𠮷");
    CHECK(Names::ComparisonKey("\xC3\x28").empty());
    CHECK(Names::ComparisonKey("[A-B] X") != Names::ComparisonKey("[AB] X"));
    CHECK(Names::OriginalPrefix("星夜礼服-上衣A", U"星夜礼服") == "星夜礼服");
    CHECK(Names::SlotNumbers(top | shoes | fx) == "32, 37, 61");
    CHECK(Names::PresetName("Wardrobe", "星夜礼服", 1, 1, 0) == "Wardrobe");
    CHECK(Names::PresetName("Wardrobe", "星夜礼服", 1, 2, 0) == "Wardrobe_1");
    CHECK(Names::PresetName("Wardrobe", "星夜礼服", 1, 2, 1) == "Wardrobe_2");
    CHECK(Names::PresetName("Wardrobe", "星夜礼服", 2, 1, 0) == "Wardrobe_星夜礼服");
    CHECK(Names::PresetName("Wardrobe", "星夜礼服", 2, 2, 0) == "Wardrobe_星夜礼服_1");
    CHECK(Names::PresetName("Wardrobe", "星夜礼服", 2, 2, 1) == "Wardrobe_星夜礼服_2");
    CHECK(Names::PresetName("Wardrobe", "", 2, 1, 0).empty());

    // ---- one style, two variants of every slot ---------------------------
    std::vector<Piece> items{ Item("星夜礼服_假发A", hair), Item("星夜礼服-上衣A", top), Item("星夜礼服 上衣Ｂ", top),
        Item("星夜礼服裙子A", skirt), Item("星夜礼服裙子B", skirt), Item("星夜礼服鞋子A", shoes) };
    auto groups = Match::Plan(items);
    CHECK(groups.size() == 1 && groups[0].prefix == U"星夜礼服" && !groups[0].issue);
    CHECK(groups[0].variants == std::vector<IndexList>({ { 0, 1, 3, 5 }, { 0, 2, 4, 5 } }));
    CHECK(groups[0].pending.empty());

    // A second style under the same first two characters stays its own style
    // instead of being matched across styles.
    items.push_back(Item("星夜晚礼服上衣A", top));
    items.push_back(Item("星夜晚礼服裙子A", skirt));
    items.push_back(Item("星夜晚礼服假发A", hair));
    groups = Match::Plan(items);
    CHECK(groups.size() == 2);
    CHECK(groups[0].prefix == U"星夜礼服" && groups[0].variants.size() == 2 && groups[0].pending.empty());
    CHECK(groups[1].prefix == U"星夜晚礼服" && groups[1].variants == std::vector<IndexList>({ { 6, 7, 8 } }));

    // ---- labels are explicit style boundaries ----------------------------
    std::vector<Piece> labelled{ Item("[Rokona] 挎包", bag), Item("［Rokona］内裤", panty), Item("[Rokona] 头发", hair),
        Item("[Rokona] 鞋袜", shoes), Item("[Rokona Extra] 上衣", top) };
    groups = Match::Plan(labelled);
    CHECK(groups.size() == 2);
    CHECK(groups[0].prefix == U"[rokona extra]" && groups[0].issue == std::string_view("no-complete-outfit"));
    CHECK(groups[1].prefix == U"[rokona]" && groups[1].variants == std::vector<IndexList>({ { 0, 1, 2, 3 } }));

    // Two parts that only share the FX bit fight for no body slot, so both belong
    // to the same outfit.
    std::vector<Piece> mixed = labelled;
    mixed.insert(mixed.end(), { Item("[Rokona] 上衣", top | fx), Item("[Rokona] 裙子", skirt | fx) });
    groups = Match::Plan(mixed);
    CHECK(groups.size() == 2);
    CHECK(groups[1].prefix == U"[rokona]" && groups[1].variants == std::vector<IndexList>({ { 0, 1, 2, 3, 5, 6 } }));
    CHECK(groups[1].pending.empty());

    // Two styles inside one label are separated by the text after the label.
    std::vector<Piece> nested{ Item("[Rokona] 衣服上身", top), Item("[Rokona] 衣服下身", skirt), Item("[Rokona] 头发", hair),
        Item("[Rokona] 鞋袜", shoes) };
    groups = Match::Plan(nested, {}, false);
    CHECK(groups.size() == 1 && groups[0].variants == std::vector<IndexList>({ { 0, 1, 2, 3 } }));
    groups = Match::Plan(nested);
    CHECK(groups.size() == 1 && groups[0].prefix == U"[rokona]" && groups[0].variants == std::vector<IndexList>({ { 0, 1, 2, 3 } }));

    std::vector<Piece> pack{ Item("[Pack] Orion Top", top), Item("[Pack] Orion Skirt", skirt), Item("[Pack] Nova Top", top),
        Item("[Pack] Nova Skirt", skirt) };
    groups = Match::Plan(pack);
    CHECK(groups.size() == 2);
    CHECK(groups[0].prefix == U"[pack]orion" && groups[0].variants == std::vector<IndexList>({ { 0, 1 } }));
    CHECK(groups[1].prefix == U"[pack]nova" && groups[1].variants == std::vector<IndexList>({ { 2, 3 } }));

    // ---- difference marks must line up -----------------------------------
    // Marks that do not line up are not paired by name. The style still gets one
    // full outfit per variant, and the second skirt is offered instead of doubling
    // every outfit with it.
    std::vector<Piece> marks{ Item("星夜礼服上衣A", top), Item("星夜礼服上衣B", top), Item("星夜礼服裙子红色", skirt),
        Item("星夜礼服裙子黑色", skirt) };
    groups = Match::Plan(marks);
    CHECK(groups.size() == 1 && !groups[0].issue);
    CHECK(groups[0].variants == std::vector<IndexList>({ { 0, 2 }, { 1, 2 } }));
    CHECK(ReasonOf(groups[0], 3) == std::string_view("same-slot-alternative"));
    CHECK_COMPLETE(groups, marks);

    Match::Aliases aliases{ { U"红色", U"a" }, { U"黑色", U"b" } };
    groups = Match::Plan(marks, aliases);
    CHECK(groups.size() == 1 && groups[0].variants == std::vector<IndexList>({ { 0, 2 }, { 1, 3 } }));
    CHECK(!groups[0].issue && groups[0].pending.empty());
    CHECK_COMPLETE(groups, marks);

    // Two options that translate to the same mark cannot be told apart either.
    Match::Aliases duplicate{ { U"红色", U"a" }, { U"黑色", U"a" } };
    groups = Match::Plan(marks, duplicate);
    CHECK(groups.size() == 1 && groups[0].variants == std::vector<IndexList>({ { 0, 2 }, { 1, 2 } }));
    CHECK(ReasonOf(groups[0], 3) == std::string_view("non-unique-difference"));

    // Two records with one and the same name cannot be told apart: the second one
    // is never pretended to be a variant of its own.
    Match::Aliases clash{ { U"红色", U"a" }, { U"黑色", U"a" } };
    std::vector<Piece> same = marks;
    same[3] = Item("星夜礼服裙子红色", skirt);
    groups = Match::Plan(same, clash);
    CHECK(groups.size() == 1 && groups[0].variants == std::vector<IndexList>({ { 0, 2 }, { 1, 2 } }));
    CHECK(ReasonOf(groups[0], 3) == std::string_view("non-unique-difference"));

    // An empty mark is a legal base variant as long as it is unique.
    std::vector<Piece> base{ Item("Love Letter Maiden Boots", shoes), Item("Love Letter Maiden Loose", top),
        Item("Love Letter Maiden Normal", top), Item("Love Letter Maiden Normal (Short)", top),
        Item("Love Letter Maiden Panty", panty), Item("Love Letter Maiden Panty (Trans)", panty) };
    groups = Match::Plan(base);
    CHECK(groups.size() == 1 && groups[0].variants.size() == 3);
    CHECK(groups[0].variants == std::vector<IndexList>({ { 0, 1, 4 }, { 0, 2, 4 }, { 0, 3, 4 } }));
    CHECK(ReasonOf(groups[0], 5) == std::string_view("same-slot-alternative"));
    CHECK_COMPLETE(groups, base);

    // ---- an option slot is drawn, not taken in plugin order --------------
    // Every preset must still hold exactly one piece of that slot, the draw must
    // really use both options, and one seed must always give the same presets.
    const auto drawsWithSeed = [&base](std::uint64_t seed) {
        Match::Rand rng{ seed };
        return Match::Plan(base, {}, true, &rng);
    };
    std::size_t firstChosen = 0, secondChosen = 0;
    for (std::uint64_t seed = 1; seed <= 8; ++seed) {
        const auto drawn = drawsWithSeed(seed);
        CHECK(drawn.size() == 1 && drawn[0].variants.size() == 3);
        CHECK_COMPLETE(drawn, base);
        for (const auto& variant : drawn[0].variants) {
            const bool first = std::binary_search(variant.begin(), variant.end(), 4);
            const bool second = std::binary_search(variant.begin(), variant.end(), 5);
            CHECK(first != second);
            first ? ++firstChosen : ++secondChosen;
        }
    }
    CHECK(firstChosen > 0 && secondChosen > 0);
    CHECK(drawsWithSeed(7)[0].variants == drawsWithSeed(7)[0].variants);

    // ---- a slot that cannot be paired does not sink the outfit -----------
    std::vector<Piece> orionAC{ Item("Orion Top A", top), Item("Orion Top B", top), Item("Orion Skirt A", skirt),
        Item("Orion Skirt C", skirt) };
    groups = Match::Plan(orionAC);
    CHECK(groups.size() == 1 && groups[0].variants == std::vector<IndexList>({ { 0, 2 }, { 1, 2 } }));
    CHECK(ReasonOf(groups[0], 3) == std::string_view("same-slot-alternative"));
    CHECK_COMPLETE(groups, orionAC);

    std::vector<Piece> orionShoes{ Item("Orion Top A", top), Item("Orion Top B", top), Item("Orion Shoes", shoes) };
    groups = Match::Plan(orionShoes);
    CHECK(groups.size() == 1 && groups[0].variants == std::vector<IndexList>({ { 0, 2 }, { 1, 2 } }));

    std::vector<Piece> five{ Item("Orion Top A", top), Item("Orion Top B", top), Item("Orion Skirt Red", skirt),
        Item("Orion Skirt Blue", skirt), Item("Orion Shoes", shoes) };
    groups = Match::Plan(five);
    CHECK(groups.size() == 1 && groups[0].variants == std::vector<IndexList>({ { 0, 2, 4 }, { 1, 2, 4 } }));
    CHECK(ReasonOf(groups[0], 3) == std::string_view("same-slot-alternative"));
    CHECK_COMPLETE(groups, five);

    // ---- real [BAKU]DOAXVV Love Letter Maiden shape ----------------------
    std::vector<Piece> maiden{ Item("Love Letter Maiden Boots", shoes), Item("Love Letter Maiden Gloves", 1u << 3),
        Item("Love Letter Maiden Loose", top), Item("Love Letter Maiden Normal", top), Item("Love Letter Maiden Sleeve", 1u << 5),
        Item("Love Letter Maiden Collar", 1u << 4) };
    groups = Match::Plan(maiden);
    CHECK(groups.size() == 1 && groups[0].variants.size() == 2 && groups[0].pending.empty());
    CHECK(groups[0].variants == std::vector<IndexList>({ { 0, 1, 2, 4, 5 }, { 0, 1, 3, 4, 5 } }));

    // ---- real [SSE] Personal Pack shape ---------------------------------
    std::vector<Piece> rabbit{ Item("[Rabbit404] 紧身衣", top), Item("[Rabbit404] 靴子", shoes), Item("[Rabbit404] 护目镜", 1u << 8),
        Item("[Rabbit404] 连帽卫衣", 1u << 6), Item("[Rokona] 挎包", bag), Item("[Rokona] 内裤", panty), Item("[Rokona] 头发", hair),
        Item("[Rokona] 鞋袜", shoes) };
    groups = Match::Plan(rabbit);
    CHECK(groups.size() == 2);
    CHECK(groups[0].prefix == U"[rabbit404]" && groups[0].variants == std::vector<IndexList>({ { 0, 1, 2, 3 } }));
    CHECK(groups[1].prefix == U"[rokona]" && groups[1].variants == std::vector<IndexList>({ { 4, 5, 6, 7 } }));

    // A piece that covers two slots is used by every variant it fits, and never
    // pushes an ordinary piece off its slot.
    std::vector<Piece> cover{ Item("Orion Top A", top | skirt), Item("Orion Top B", top | skirt), Item("Orion Boots", shoes) };
    groups = Match::Plan(cover);
    CHECK(groups.size() == 1 && groups[0].variants == std::vector<IndexList>({ { 0, 2 }, { 1, 2 } }));

    // A part that covers two slots and a part that covers one of them cannot be
    // worn together, and the style can never be filled: nothing is claimed.
    std::vector<Piece> coverSkirt = cover;
    coverSkirt.push_back(Item("Orion Skirt", skirt));
    groups = Match::Plan(coverSkirt);
    CHECK(groups.size() == 1 && groups[0].variants.empty());
    CHECK(groups[0].issue == std::string_view("overlapping-slot-masks"));
    CHECK(ReasonOf(groups[0], 0) == std::string_view("overlapping-slot-masks"));
    CHECK(ReasonOf(groups[0], 1) == std::string_view("overlapping-slot-masks"));
    CHECK(ReasonOf(groups[0], 3) == std::string_view("incomplete-slots"));

    // A single piece is its own outfit; unrelated names are left alone.
    groups = Match::Plan({ Item("Orion Dress", top | skirt) });
    CHECK(groups.size() == 1 && groups[0].variants == std::vector<IndexList>({ { 0 } }));

    CHECK(Match::Plan({ Item("Orion Top", top | skirt), Item("Orion Skirt", skirt) }).size() == 1);
    groups = Match::Plan({ Item("Orion Top", top | skirt), Item("Orion Skirt", skirt) });
    CHECK(groups[0].issue == std::string_view("overlapping-slot-masks"));

    CHECK(Match::Plan({ Item("星夜礼服上衣A", top), Item("Dawn Skirt", skirt) }).empty());
    CHECK(Match::Plan({ Item("Dark Top", top), Item("Dawn Skirt", skirt) }).empty());

    groups = Match::Plan({ Item("Orion Night Top", top), Item("Orion Night Skirt", skirt), Item("Orion Shoes", shoes) });
    CHECK(groups.size() == 1 && groups[0].variants == std::vector<IndexList>({ { 0, 1, 2 } }));

    // Too many variants on one slot: keep the limit, report the rest.
    std::vector<Piece> many{ Item("Orion Shoes", shoes) };
    for (int i = 0; i <= 32; ++i) {
        many.push_back(Item("Orion Top " + std::to_string(i), top));
    }
    groups = Match::Plan(many);
    CHECK(groups.size() == 1);
    CHECK(groups[0].variants.size() == Match::kMaxVariants);
    CHECK(groups[0].variants[0] == IndexList({ 0, 1 }));
    CHECK(groups[0].pending.size() == 1 && groups[0].pending[0].second == std::string_view("variant-limit"));

    std::cout << "ESP outfit matching: all checks passed\n";
    return 0;
}
