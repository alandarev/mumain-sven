#include <doctest.h>
#include "UI/AuctionHouse/BrowseQuery.h"
#include "UI/AuctionHouse/BrowseState.h"
#include "UI/AuctionHouse/EquipmentDetails.h"
#include <algorithm>

using namespace UI::AuctionHouse;

namespace
{
MarketComparison DragonArmor()
{
    MarketComparison comparison;
    comparison.subject = "Excellent Dragon Armor +9";
    comparison.type = 4105;
    comparison.level = 9;
    comparison.baseName = "Dragon Armor";
    comparison.sellSlot = 24;
    comparison.sellKey = 77;
    return comparison;
}
} // namespace

TEST_CASE("Auction categories keep their own name and filters [auction][ui]")
{
    BrowseState state;
    BrowseQuery armor;
    armor.search = "Dragon Armor";
    armor.usable = true;
    armor.filters.family = "2";
    // Equipment "Dragon Armor" → Jewels: the jewels list is not filtered by it.
    const auto jewels = state.SwitchTo(Category::Jewels, armor);
    CHECK(state.Active() == Category::Jewels);
    CHECK(jewels.search.empty());
    CHECK_FALSE(HasConstraints(jewels));
    BrowseQuery bless;
    bless.search = "bless";
    CHECK(state.SwitchTo(Category::Materials, bless).search.empty());
    BrowseQuery materials;
    materials.search = "chaos";
    const auto back = state.SwitchTo(Category::Equipment, materials);
    CHECK(back.search == "Dragon Armor");
    CHECK(back.usable);
    CHECK(back.filters.family == "2");
    CHECK(state.SwitchTo(Category::Jewels, back).search == "bless");
    CHECK(state.SwitchTo(Category::Materials, bless).search == "chaos");
}

TEST_CASE("Auction empty result names the active search and its category [auction][ui]")
{
    BrowseQuery query;
    query.search = "Dragon Armor";
    CHECK(EmptyResultText(Category::Jewels, query) == "No jewels match \"Dragon Armor\"");
    CHECK(EmptyResultText(Category::Equipment, query) == "No equipment matches \"Dragon Armor\"");
    CHECK(EmptyResultText(Category::Materials, query) == "No materials match \"Dragon Armor\"");
    query.search.clear();
    CHECK(EmptyResultText(Category::Equipment, query) == "No equipment listed");
    query.filters.luck = true;
    CHECK(HasConstraints(query));
    CHECK(EmptyResultText(Category::Equipment, query) == "No equipment matches these filters");
    query.filters.luck = false;
    query.sort = "2";
    CHECK_FALSE(HasConstraints(query));
}

TEST_CASE("Auction market comparison uses a clean same-base query and gives the player's queries back [auction][ui]")
{
    BrowseState state;
    BrowseQuery own;
    own.search = "Blade";
    own.usable = true;
    own.option = "3";
    own.filters.classLine = "1";
    own.filters.maxPrice = "5kk";
    BrowseQuery jewels;
    jewels.search = "soul";
    state.SwitchTo(Category::Jewels, own);
    // The player is browsing jewels when the Sell tab starts a comparison.
    const auto compared = state.BeginComparison(jewels, DragonArmor());
    CHECK(state.Active() == Category::Equipment);
    CHECK(compared.search == "Dragon Armor");
    CHECK(compared.filters.upgradeMin == "9");
    CHECK(compared.filters.upgradeMax == "9");
    CHECK_FALSE(compared.usable);
    CHECK(compared.option == "0");
    CHECK(compared.equipment == "0");
    CHECK(compared.filters.classLine == "255");
    CHECK(compared.filters.maxPrice.empty());
    CHECK(compared.filters.family == "0");
    CHECK(state.ComparisonShown());
    REQUIRE(state.ComparedType("Dragon Armor"));
    CHECK(*state.ComparedType("Dragon Armor") == 4105);
    // A changed name is the player's own search again.
    CHECK_FALSE(state.ComparedType("Dragon"));
    // Switching categories during the comparison hides its banner, keeps it and the jewels query.
    auto refined = compared;
    refined.filters.family = "2";
    CHECK(state.SwitchTo(Category::Jewels, refined).search == "soul");
    CHECK_FALSE(state.ComparisonShown());
    CHECK_FALSE(state.ComparedType("Dragon Armor"));
    CHECK(state.SwitchTo(Category::Equipment, jewels).filters.family == "2");
    CHECK(state.ComparisonShown());
    REQUIRE(state.Comparison());
    CHECK(state.Comparison()->sellSlot == 24);
    CHECK(state.Comparison()->sellKey == 77);
    CHECK(state.Comparison()->subject == "Excellent Dragon Armor +9");
    // Ending it restores the category and every query from before it.
    const auto restored = state.EndComparison();
    CHECK_FALSE(state.Comparison());
    CHECK(state.Active() == Category::Jewels);
    CHECK(restored.search == "soul");
    const auto equipment = state.SwitchTo(Category::Equipment, restored);
    CHECK(equipment.search == "Blade");
    CHECK(equipment.usable);
    CHECK(equipment.option == "3");
    CHECK(equipment.filters.classLine == "1");
    CHECK(equipment.filters.maxPrice == "5kk");
}

TEST_CASE("Auction a second comparison keeps the queries from before the first [auction][ui]")
{
    BrowseState state;
    BrowseQuery own;
    own.search = "Blade";
    state.BeginComparison(own, DragonArmor());
    auto other = DragonArmor();
    other.baseName = "Dragon Helm";
    other.type = 3593;
    CHECK(state.BeginComparison(state.SwitchTo(Category::Equipment, {}), other).search == "Dragon Helm");
    CHECK(state.EndComparison().search == "Blade");
}

TEST_CASE("Auction removing a family clears the choices that depend on it [auction][ui]")
{
    AdvancedFilters filters;
    filters.family = "0";
    filters.ancientSet = "Hyon";
    filters.minSockets = "3";
    filters.option = "4";
    filters.minExcellent = "2";
    ClearFamilyDependants(filters);
    CHECK(filters.ancientSet.empty());
    CHECK(filters.minSockets == "0");
    CHECK(filters.option == "4");
    CHECK(filters.minExcellent == "2");
    filters.family = "3";
    filters.ancientSet = "Hyon";
    filters.minSockets = "3";
    ClearFamilyDependants(filters);
    CHECK(filters.ancientSet == "Hyon");
    CHECK(filters.minSockets == "0");
    filters.family = "1";
    ClearFamilyDependants(filters);
    CHECK(filters.ancientSet.empty());
    CHECK(filters.option == "0");
    CHECK(filters.minExcellent == "0");
    filters.family = "4";
    filters.minSockets = "2";
    ClearFamilyDependants(filters);
    CHECK(filters.minSockets == "2");
}

TEST_CASE("Auction Filters dialog says why it cannot be applied [auction][ui]")
{
    GameLogic::AuctionHouse::Rules rules;
    rules.maxPrice = 2000000000;
    rules.priceStepPrecision = 2;
    AdvancedFilters filters;
    CHECK(FilterProblem(filters, false, rules).empty());
    filters.upgradeMin = "11";
    filters.upgradeMax = "7";
    CHECK(FilterProblem(filters, false, rules) == "Upgrade min +11 is above max +7.");
    filters.upgradeMax = "11";
    filters.maxPrice = "1.000kk";
    CHECK(FilterProblem(filters, false, rules).starts_with("Max price: enter a whole number or"));
    filters.maxPrice = "0";
    CHECK_FALSE(FilterProblem(filters, false, rules).empty());
    filters.maxPrice = "1.5kk";
    CHECK(FilterProblem(filters, false, rules).empty());
    filters.family = "1";
    CHECK(FilterProblem(filters, true, rules).starts_with("Normal items have no excellent options"));
    CHECK(FilterProblem(filters, false, rules).empty());
    filters.minExcellent = "2";
    CHECK_FALSE(FilterProblem(filters, false, rules).empty());
}

TEST_CASE("Auction Filters price hint only suggests prices the filter accepts [auction][ui]")
{
    // UX3-02: the hint recommended 2.3b while the default cap is 2,000,000,000.
    GameLogic::AuctionHouse::Rules rules;
    rules.priceStepPrecision = 2;
    for (const GameLogic::AuctionHouse::Zen cap : {2000000000u, 5000000u, 800000u})
    {
        INFO(cap);
        rules.maxPrice = cap;
        AdvancedFilters filters;
        filters.maxPrice = "3b";
        const auto problem = FilterProblem(filters, false, rules);
        REQUIRE(problem.starts_with("Max price: enter a whole number or "));
        CHECK(problem.ends_with(", up to " + ExactZen(cap) + "."));
        CHECK(problem.find("2.3b") == std::string::npos);
        // Every example in it is accepted by the same filter.
        auto examples = problem.substr(std::string("Max price: enter a whole number or ").size());
        examples = examples.substr(0, examples.find(", up to"));
        std::size_t start = 0;
        int count = 0;
        while (start < examples.size())
        {
            auto end = examples.find(" · ", start);
            if (end == std::string::npos)
                end = examples.size();
            filters.maxPrice = examples.substr(start, end - start);
            INFO(filters.maxPrice);
            const auto price = ParseZen(filters.maxPrice, cap);
            REQUIRE(price);
            CHECK(*price <= cap);
            CHECK(FilterProblem(filters, false, rules).empty());
            ++count;
            start = end == examples.size() ? end : end + std::string(" · ").size();
        }
        CHECK(count >= 1);
    }
}

TEST_CASE("Auction class labels include evolutions [auction][ui]")
{
    CHECK(ClassLineLabel("1") == "Dark Knight (including evolutions)");
    CHECK(ClassLineLabel("6") == "Rage Fighter (including evolutions)");
    CHECK(ClassLineLabel("255").empty());
    CHECK(ClassLineLabel("7").empty());
}

TEST_CASE("Auction rows keep every short excellent option and mark the searched ones [auction][ui]")
{
    constexpr std::uint16_t weapon = 1u << 0, armor = 1u << 3, wings = 1u << 7;
    auto marks = ExcellentMarks(weapon, 0x09);
    MarkSearchedOptions(marks, 0x08);
    REQUIRE(marks.size() == 2);
    CHECK(marks[0].label == "Damage +2%");
    CHECK(marks[0].matched);
    CHECK(marks[1].label == "Mana on kill");
    CHECK_FALSE(marks[1].matched);
    // The same bits on armour are its defensive options.
    auto defense = ExcellentMarks(armor, 0x3f);
    MarkSearchedOptions(defense, 0x20);
    REQUIRE(defense.size() == 6);
    CHECK(defense[0].label == "Defense success rate +10%");
    CHECK(defense[4].label == "Maximum HP +4%");
    CHECK(defense[4].matched);
    CHECK(std::count_if(defense.begin(), defense.end(), [](const auto& mark) { return mark.matched; }) == 1);
    CHECK(ExcellentMarks(wings, 0x3f).empty());
}

TEST_CASE("Auction Cannot Equip says why: the class or each stat against the character [auction][ui]")
{
    EquipFacts facts;
    facts.requireClass = {0, 1, 0, 1, 0, 0, 0};
    facts.baseClass = 1;
    facts.stats = {{"Strength", 419, 300}, {"Agility", 80, 120}, {"Level", 380, 400}};
    auto reasons = CannotEquipReasons(facts);
    REQUIRE(reasons.size() == 1);
    CHECK(reasons[0] == "Strength 419 · you have 300");
    facts.baseClass = 0;
    reasons = CannotEquipReasons(facts);
    REQUIRE(reasons.size() == 2);
    CHECK(reasons[0] == "For Dark Knight, Magic Gladiator only");
    // A Magic Gladiator wears what both the Dark Wizard and the Dark Knight wear.
    facts.requireClass = {1, 1, 0, 0, 0, 0, 0};
    facts.baseClass = 3;
    CHECK(CannotEquipReasons(facts).size() == 1);
    // Wearable from the second class on.
    facts.requireClass = {0, 2, 0, 0, 0, 0, 0};
    facts.baseClass = 1;
    facts.classStep = 1;
    facts.stats = {{"Strength", 1200, 1000}};
    reasons = CannotEquipReasons(facts);
    REQUIRE(reasons.size() == 2);
    CHECK(reasons[0] == "Needs the 2nd class");
    CHECK(reasons[1] == "Strength 1,200 · you have 1,000");
    facts.classStep = 2;
    facts.stats.clear();
    CHECK(CannotEquipReasons(facts).empty());
}
