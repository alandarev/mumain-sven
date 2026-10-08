#include <doctest.h>
#include "UI/AuctionHouse/GoodsQuote.h"
#include "UI/AuctionHouse/RequestClock.h"

#include <vector>

using namespace UI::AuctionHouse;

namespace
{
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Market::Quote MakeQuote(std::uint32_t requested, std::vector<Market::PriceTier> tiers, std::uint32_t fillable,
                        Market::Zen total)
{
    Market::Quote quote;
    quote.requestedQuantity = requested;
    quote.fillableQuantity = fillable;
    quote.total = total;
    quote.tiers = std::move(tiers);
    quote.result = fillable >= requested ? Market::Result::Success : Market::Result::InsufficientStock;
    return quote;
}
} // namespace

TEST_CASE("Auction order split fills the shown tiers cheapest first [auction][goods-quote]")
{
    const auto split = SplitOrder(MakeQuote(8, {{590000, 6}, {595000, 4}}, 8, 4730000));
    REQUIRE(split.tiers.size() == 2);
    CHECK(split.tiers[0].inOrder == 6);
    CHECK(split.tiers[1].inOrder == 2);
    CHECK(split.tiers[1].stock == 4);
    CHECK(split.beyondQuantity == 0);
    CHECK(OrderSummaryText(split) == "8 for 4,730,000 Zen");
    CHECK(BeyondTiersText(split).empty());
    CHECK(ShortfallText(split).empty());
    const auto lines = OrderSplitLines(split);
    REQUIRE(lines.size() == 2);
    CHECK(lines[0] == "6 × 590,000 = 3,540,000");
    CHECK(lines[1] == "2 × 595,000 = 1,190,000");
}

TEST_CASE("Auction order split names units beyond eight shown tiers [auction][goods-quote]")
{
    // Eight tiers of 1 unit from 100 to 800; the order of 10 takes 2 more units at 900 each.
    std::vector<Market::PriceTier> tiers;
    Market::Zen shown = 0;
    for (Market::Zen price = 100; price <= 800; price += 100)
    {
        tiers.push_back({price, 1});
        shown += price;
    }
    const auto split = SplitOrder(MakeQuote(10, tiers, 10, shown + 1800));
    REQUIRE(split.tiers.size() == MaximumQuoteTiers);
    for (const auto& tier : split.tiers)
        CHECK(tier.inOrder == 1);
    CHECK(split.beyondQuantity == 2);
    CHECK(split.beyondTotal == 1800);
    CHECK(BeyondTiersText(split) == "+2 at higher prices · 1,800 Zen");
    const auto lines = OrderSplitLines(split);
    REQUIRE(lines.size() == 9);
    CHECK(lines.back() == "+2 at higher prices · 1,800 Zen");
    CHECK(OrderSummaryText(split) == "10 for 5,400 Zen");
}

TEST_CASE("Auction order split says how much is available when stock is short [auction][goods-quote]")
{
    const auto split = SplitOrder(MakeQuote(50, {{590000, 6}, {595000, 24}}, 30, 17820000));
    CHECK(split.fillable == 30);
    CHECK(split.tiers[0].inOrder == 6);
    CHECK(split.tiers[1].inOrder == 24);
    CHECK(ShortfallText(split) == "Only 30 available");
    CHECK(OrderSummaryText(split) == "30 for 17,820,000 Zen");
    // A small order leaves the dearer tier out of the order.
    const auto small = SplitOrder(MakeQuote(3, {{590000, 6}, {595000, 24}}, 3, 1770000));
    CHECK(small.tiers[0].inOrder == 3);
    CHECK(small.tiers[1].inOrder == 0);
    CHECK(OrderSplitLines(small).size() == 1);
}

TEST_CASE("Auction selling line shows the stock at the lowest price and the quoted total [auction][goods-quote]")
{
    const std::vector<Market::PriceTier> tiers{{600000, 20}, {605000, 30}};
    CHECK(MarketLowestText(600000, tiers) == "Lowest 600,000 · 20 in stock · 50 total");
    CHECK(MarketLowestText(600000, std::vector<Market::PriceTier>{{600000, 20}}) == "Lowest 600,000 · 20 in stock");
    std::vector<Market::PriceTier> full(MaximumQuoteTiers, {700000, 10});
    full.front() = {600000, 20};
    CHECK(MarketLowestText(600000, full) == "Lowest 600,000 · 20 in stock · 90+ total");
    // The quote predates a new lowest price: no stock claim.
    CHECK(MarketLowestText(595000, tiers) == "Lowest 595,000");
    CHECK(MarketLowestText(0, tiers) == "No other sellers");
}

TEST_CASE("Auction quote comparison marks new tiers and stock changes [auction][goods-quote]")
{
    const std::vector<Market::PriceTier> before{{595000, 4}, {600000, 6}};
    const std::vector<Market::PriceTier> after{{590000, 6}, {595000, 4}, {600000, 10}};
    const auto changes = CompareTiers(before, after);
    REQUIRE(changes.size() == 3);
    CHECK(changes[0].added);
    CHECK_FALSE(changes[1].added);
    CHECK_FALSE(changes[1].stockChanged);
    CHECK(changes[2].stockChanged);
    CHECK(changes[2].previousStock == 6);
    CHECK(AnyTierChange(changes));
    CHECK_FALSE(AnyTierChange(CompareTiers(before, before)));
    CHECK(StockChangeText(4, 10) == "4 → 10");
    CHECK(PriceChangeText(595000, 590000) == "595k → 590k");
    CHECK(QuoteAgeText(std::chrono::seconds(0)) == "Updated just now");
    CHECK(QuoteAgeText(std::chrono::seconds(3)) == "Updated 3 s ago");
    CHECK(QuoteAgeText(std::chrono::seconds(125)) == "Updated 2 min ago");
}

TEST_CASE("Auction live goods quote polls every five seconds and pauses [auction][goods-quote]")
{
    // The window's loop: a 100 ms frame, the shared request clock, an answer 200 ms after each request,
    // and a review open from 30 s to 45 s. Two idle minutes must stay at about one request per 5 s.
    QuoteRefresh refresh;
    RequestClock clock;
    const auto start = RequestClock::Time{} + std::chrono::seconds(100);
    refresh.Schedule(start);
    std::vector<RequestClock::Time> sent;
    RequestClock::Time answerAt{};
    for (int ms = 0; ms <= 120000; ms += 100)
    {
        const auto now = start + std::chrono::milliseconds(ms);
        if (answerAt != RequestClock::Time{} && now >= answerAt)
        {
            answerAt = {};
            refresh.Schedule(now);
        }
        const bool reviewing = ms >= 30000 && ms < 45000;
        const bool inFlight = answerAt != RequestClock::Time{};
        if (refresh.Due(now, reviewing || inFlight) && clock.Ready(now))
        {
            clock.Sent(now);
            refresh.Schedule(now);
            sent.push_back(now);
            answerAt = now + std::chrono::milliseconds(200);
        }
    }
    CHECK(sent.size() >= 18);
    CHECK(sent.size() <= 24);
    for (std::size_t i = 1; i < sent.size(); ++i)
        CHECK(sent[i] - sent[i - 1] >= QuoteRefresh::Interval);
    for (const auto at : sent)
    {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(at - start).count();
        CHECK_FALSE((ms >= 30000 && ms < 45000));
    }
    refresh.Stop();
    CHECK_FALSE(refresh.Due(start + std::chrono::hours(1), false));
}
