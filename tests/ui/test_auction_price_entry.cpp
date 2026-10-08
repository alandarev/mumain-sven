#include <doctest.h>
#include "UI/AuctionHouse/PriceEntry.h"
#include "UI/AuctionHouse/Presentation.h"
#include <limits>

using namespace UI::AuctionHouse;

namespace
{
Market::Rules MarketRules(std::uint8_t precision = 2)
{
    Market::Rules rules;
    rules.priceStepPrecision = precision;
    rules.maxPrice = 2000000000;
    return rules;
}
} // namespace

TEST_CASE("Auction price shorthand allows at most two decimals after a suffix [auction][price-entry]")
{
    constexpr auto limit = std::numeric_limits<Market::Zen>::max();
    CHECK(ParseZen("590k", limit) == 590000);
    CHECK(ParseZen("1.5kk", limit) == 1500000);
    CHECK(ParseZen("1.25m", limit) == 1250000);
    CHECK(ParseZen("93,240,000", limit) == 93240000);
    CHECK_FALSE(ParseZen("1.000kk", limit));
    CHECK_FALSE(ParseZen("1.234k", limit));
    CHECK_FALSE(ParseZen("93.240.000", limit));

    PriceEntry entry;
    const auto rules = MarketRules();
    REQUIRE(entry.Edit("1.000kk"));
    CHECK_FALSE(entry.Commit(rules));
    CHECK(entry.Text() == "1.000kk");
    CHECK(entry.Note() == PriceFormatHelp);
    const auto steps = entry.Steps(rules, 0);
    CHECK_FALSE(steps.down);
    CHECK_FALSE(steps.up);
    CHECK(steps.reason == PriceFormatHelp);
}

TEST_CASE("Auction price commit rounds down with a hint and never while typing [auction][price-entry]")
{
    const auto rules = MarketRules();
    PriceEntry entry;
    REQUIRE(entry.Edit("93,240,000"));
    CHECK(entry.Text() == "93,240,000");
    CHECK(entry.Note().empty());
    CHECK(entry.Commit(rules) == 93000000);
    CHECK(entry.Text() == "93,000,000");
    CHECK(entry.Note() == "Rounded to 93,000,000");
    CHECK_FALSE(entry.Edit("93,000,000")); // The field echoing the committed value is not an edit.
    CHECK(entry.Note() == "Rounded to 93,000,000");
    CHECK(entry.Commit(rules) == 93000000);
    CHECK(entry.Note() == "Rounded to 93,000,000");
    REQUIRE(entry.Edit("93,000,0001"));
    CHECK(entry.Note().empty());

    REQUIRE(entry.Edit("29932843"));
    CHECK(entry.Commit(rules) == 29800000);
    REQUIRE(entry.Edit("594k"));
    CHECK(entry.Commit(rules) == 590000);
    CHECK(entry.Text() == "590,000");
    REQUIRE(entry.Edit("1.5kk"));
    CHECK(entry.Commit(rules) == 1500000);
    CHECK(entry.Text() == "1,500,000");
    CHECK(entry.Note().empty());
    REQUIRE(entry.Edit("2.1b"));
    CHECK_FALSE(entry.Commit(rules));
    CHECK(entry.Note() == "The maximum price is 2,000,000,000 Zen.");
    REQUIRE(entry.Edit(""));
    CHECK_FALSE(entry.Commit(rules));
    CHECK(entry.Note().empty());
}

TEST_CASE("Auction price stepping moves between valid prices with Shift tens and limits [auction][price-entry]")
{
    const auto rules = MarketRules();
    PriceEntry entry;
    REQUIRE(entry.Edit("8,900"));
    REQUIRE(entry.Step(rules, {true, 1}, 0));
    CHECK(entry.Text() == "8,950");
    REQUIRE(entry.Step(rules, {false, 1}, 0));
    REQUIRE(entry.Step(rules, {false, 1}, 0));
    CHECK(entry.Text() == "8,850");
    REQUIRE(entry.Edit("93240000"));
    REQUIRE(entry.Step(rules, {true, 1}, 0));
    CHECK(entry.Text() == "93,500,000");
    REQUIRE(entry.Edit("93240000"));
    REQUIRE(entry.Step(rules, {false, 1}, 0));
    CHECK(entry.Text() == "93,000,000");
    REQUIRE(entry.Edit("600k"));
    REQUIRE(entry.Step(rules, {true, ShiftPriceSteps}, 0));
    CHECK(entry.Text() == "650,000");
    REQUIRE(entry.Step(rules, {false, ShiftPriceSteps}, 0));
    CHECK(entry.Text() == "600,000");

    REQUIRE(entry.Edit("1"));
    auto steps = entry.Steps(rules, 0);
    CHECK_FALSE(steps.down);
    CHECK(steps.up);
    CHECK(steps.reason == "This is the lowest price.");
    CHECK_FALSE(entry.Step(rules, {false, 1}, 0));
    CHECK(entry.Text() == "1");
    REQUIRE(entry.Edit("2b"));
    steps = entry.Steps(rules, 0);
    CHECK(steps.down);
    CHECK_FALSE(steps.up);
    CHECK(steps.reason == "The maximum price is 2,000,000,000 Zen.");
    CHECK_FALSE(entry.Step(rules, {true, 1}, 0));

    auto unrestricted = MarketRules(0);
    REQUIRE(entry.Edit("8900"));
    REQUIRE(entry.Step(unrestricted, {true, 1}, 0));
    CHECK(entry.Text() == "8,901");
}

TEST_CASE("Auction empty price field steps from the lowest goods price or explains why not [auction][price-entry]")
{
    const auto rules = MarketRules();
    PriceEntry entry;
    auto steps = entry.Steps(rules, 0);
    CHECK_FALSE(steps.down);
    CHECK_FALSE(steps.up);
    CHECK(steps.reason == "Type a price first.");
    CHECK_FALSE(entry.Step(rules, {true, 1}, 0));

    steps = entry.Steps(rules, 600000);
    CHECK(steps.down);
    CHECK(steps.up);
    REQUIRE(entry.Step(rules, {true, 1}, 600000));
    CHECK(entry.Text() == "600,000");
    entry.Reset();
    REQUIRE(entry.Step(rules, {false, 1}, 600000));
    CHECK(entry.Text() == "595,000");
    entry.Reset();
    REQUIRE(entry.Step(rules, {false, 1}, 594000));
    CHECK(entry.Text() == "590,000");
}

TEST_CASE("Auction Match lowest and Undercut use the lowest other-seller price [auction][price-entry]")
{
    const auto rules = MarketRules();
    CHECK(UndercutPrice(rules, 600000) == 595000);
    CHECK(UndercutPrice(rules, 595000) == 590000);
    CHECK(UndercutPrice(rules, 594000) == 590000);
    CHECK_FALSE(UndercutPrice(rules, 0));
    CHECK_FALSE(UndercutPrice(rules, 1));

    PriceEntry entry;
    entry.Choose(rules, 600000);
    CHECK(entry.Text() == "600,000");
    CHECK(entry.Edited());
    CHECK(entry.Source() == PriceSource::Typed);
    entry.Choose(rules, 594000);
    CHECK(entry.Text() == "590,000");
    CHECK(entry.Note() == "Rounded to 590,000");
}

TEST_CASE("Auction price prefill prefers draft then remembered then lowest and keeps edits [auction][price-entry]")
{
    const auto rules = MarketRules();
    PriceEntry entry;
    REQUIRE(entry.Prefill(rules, std::string_view("12,500,000"), 590000, 600000));
    CHECK(entry.Text() == "12,500,000");
    CHECK(entry.Source() == PriceSource::Typed);
    CHECK(entry.Edited());

    entry.Reset();
    REQUIRE(entry.Prefill(rules, std::nullopt, 590000, 600000));
    CHECK(entry.Text() == "590,000");
    CHECK(entry.Source() == PriceSource::Remembered);
    CHECK(PriceSourceLabel(entry.Source()) == "Last price");
    CHECK_FALSE(entry.Edited());

    entry.Reset();
    REQUIRE(entry.Prefill(rules, std::nullopt, 594000, 0)); // Remembered under an older rule.
    CHECK(entry.Text() == "590,000");

    entry.Reset();
    REQUIRE(entry.Prefill(rules, std::nullopt, std::nullopt, 600000));
    CHECK(entry.Text() == "600,000");
    CHECK(PriceSourceLabel(entry.Source()) == "Current lowest");
    // A later answer may move an untouched lowest price...
    REQUIRE(entry.Prefill(rules, std::nullopt, std::nullopt, 590000));
    CHECK(entry.Text() == "590,000");
    // ...but never a price the player has edited.
    REQUIRE(entry.Edit("610,000"));
    CHECK(entry.Source() == PriceSource::Typed);
    CHECK(PriceSourceLabel(entry.Source()).empty());
    CHECK_FALSE(entry.Prefill(rules, std::nullopt, 595000, 580000));
    CHECK(entry.Text() == "610,000");

    entry.Reset();
    CHECK_FALSE(entry.Prefill(rules, std::nullopt, std::nullopt, 0));
    CHECK(entry.Text().empty());
    CHECK(entry.Source() == PriceSource::None);
}
