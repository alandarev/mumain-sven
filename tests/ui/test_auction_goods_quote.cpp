#include <doctest.h>
#include "UI/AuctionHouse/GoodsQuote.h"
#include "UI/AuctionHouse/RequestClock.h"

#include <algorithm>
#include <optional>
#include <span>
#include <utility>
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

TEST_CASE("Auction availability follows each quote [auction][goods-quote]")
{
    // Fewer than eight tiers carry the whole stock: the live finding (list said 50, tiers held 56).
    CHECK(AvailableAfterQuote(MakeQuote(8, {{590000, 6}, {600000, 20}, {650000, 30}}, 8, 4740000)) ==
          GoodsAvailability{56, true});
    CHECK(AvailableAfterQuote(MakeQuote(1, {{600000, 19}, {650000, 30}}, 1, 600000)) == GoodsAvailability{49, true});
    // A shortfall answer is exact, whatever the tiers.
    CHECK(AvailableAfterQuote(MakeQuote(999, {{600000, 18}, {650000, 30}}, 48, 0)) == GoodsAvailability{48, true});
    // Nothing for sale.
    CHECK(AvailableAfterQuote(MakeQuote(1, {}, 0, 0)) == GoodsAvailability{0, true});
    CHECK_FALSE(GoodsCountNeeded(AvailableAfterQuote(MakeQuote(1, {{600000, 19}}, 1, 600000))));
}

TEST_CASE("Auction eight tiers give a lower bound until the goods list counts hidden tiers [auction][goods-quote]")
{
    std::vector<Market::PriceTier> eight;
    eight.reserve(MaximumQuoteTiers);
    for (Market::Zen i = 0; i < MaximumQuoteTiers; ++i)
        eight.push_back({1000000 + 50000 * i, 5});
    const auto capped = AvailableAfterQuote(MakeQuote(1, eight, 1, 1000000));
    CHECK(capped == GoodsAvailability{40, false});
    CHECK(GoodsCountNeeded(capped));
    // A larger order fills from hidden tiers: the fillable quantity raises the bound.
    CHECK(AvailableAfterQuote(MakeQuote(55, eight, 55, 60000000)) == GoodsAvailability{55, false});

    // The goods list counts every tier: 70 while hidden tiers hold 30 more.
    const std::vector<Market::GoodsRow> before{{7181, 0, 1000000, 70, 9}, {7181, 1, 9000000, 3, 1}};
    CHECK(AvailableFromGoodsList({before, 2, 0}, 7181, 0) == GoodsAvailability{70, true});
    CHECK(AvailableFromGoodsList({before, 2, 0}, 7181, 1) == GoodsAvailability{3, true});
    // Hidden tiers sold out: the count drops to the shown 40, it is not kept at the earlier 70.
    const std::vector<Market::GoodsRow> removed{{7181, 0, 1000000, 40, 8}};
    CHECK(AvailableFromGoodsList({removed, 1, 0}, 7181, 0) == GoodsAvailability{40, true});
    // Hidden tiers grew: the count follows to 95.
    const std::vector<Market::GoodsRow> grown{{7181, 0, 1000000, 95, 14}};
    CHECK(AvailableFromGoodsList({grown, 1, 0}, 7181, 0) == GoodsAvailability{95, true});
    // The last listing went: none for sale.
    CHECK(AvailableFromGoodsList({{}, 0, 0}, 7181, 0) == GoodsAvailability{0, true});
}

namespace
{
// The server's goods list for one type (AuctionMarketReader.GetGoodsList): rows ordered by level,
// pages of PageSize (at least 1), TotalCount over all pages.
struct GoodsDirectory
{
    std::vector<Market::GoodsRow> rows;
    std::uint8_t pageSize = 20;
    int requests = 0;

    std::vector<Market::GoodsRow> Page(std::uint16_t page)
    {
        ++requests;
        const std::size_t size = std::max<std::size_t>(1, pageSize);
        const auto first = std::min(rows.size(), page * size);
        const auto last = std::min(rows.size(), first + size);
        return {rows.begin() + static_cast<std::ptrdiff_t>(first), rows.begin() + static_cast<std::ptrdiff_t>(last)};
    }
};

// What Window::ApplyGoodsCount does with each answer: the exact count, or the next page.
std::optional<GoodsAvailability> CountThroughPages(GoodsDirectory& directory, std::uint8_t level)
{
    GoodsCountPaging paging;
    paging.Start(7181, level);
    while (true)
    {
        const auto rows = directory.Page(paging.Page());
        if (const auto exact = paging.Apply(rows, static_cast<std::uint32_t>(directory.rows.size())))
            return exact;
        if (!paging.More())
            return std::nullopt;
    }
}
} // namespace

TEST_CASE("Auction a goods-list count below the quote's lower bound stays a lower bound [auction][goods-quote]")
{
    // RR-02 under churn: page 0 holds level 0, level 0 vanishes before page 1, page 1 is empty, so the
    // list says none for sale while the quote showed eight full tiers of the selected level.
    const auto none = MergeGoodsCount(40, GoodsAvailability{0, true});
    CHECK(none.count == 40);
    CHECK_FALSE(none.exact);
    // A stale row below the quote is not taken as exact either.
    CHECK_FALSE(MergeGoodsCount(40, GoodsAvailability{35, true}).exact);
    // At or above the lower bound the list count is the exact availability.
    const auto equal = MergeGoodsCount(40, GoodsAvailability{40, true});
    CHECK(equal.count == 40);
    CHECK(equal.exact);
    const auto more = MergeGoodsCount(40, GoodsAvailability{95, true});
    CHECK(more.count == 95);
    CHECK(more.exact);
}

TEST_CASE("Auction exact availability never reads a partial goods-list page as none for sale [auction][goods-quote]")
{
    // RR-02: three levels of Jewel of Bless; the selected level 1 sits on page 1 with PageSize 1.
    const std::vector<Market::GoodsRow> levels{
        {7181, 0, 590000, 70, 9}, {7181, 1, 5900000, 12, 3}, {7181, 3, 9000000, 2, 1}};
    // One page alone does not say level 1 is missing: the count stays a lower bound.
    CHECK_FALSE(AvailableFromGoodsList({std::span(levels).first(1), 3, 0}, 7181, 1));
    for (const std::uint8_t pageSize : {std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{20}, std::uint8_t{0}})
    {
        INFO(int(pageSize));
        GoodsDirectory directory{levels, pageSize};
        CHECK(CountThroughPages(directory, 1) == GoodsAvailability{12, true});
        directory.requests = 0;
        CHECK(CountThroughPages(directory, 3) == GoodsAvailability{2, true});
        // Level 2 has nothing: none for sale once the list passes it (level 3) ...
        CHECK(CountThroughPages(directory, 2) == GoodsAvailability{0, true});
        // ... or ends before it.
        CHECK(CountThroughPages(directory, 9) == GoodsAvailability{0, true});
    }
    // With PageSize 1 the selected level 1 is the second request; level 3 the third.
    GoodsDirectory single{levels, 1};
    CHECK(CountThroughPages(single, 1) == GoodsAvailability{12, true});
    CHECK(single.requests == 2);

    // Bounded: a type has at most 16 levels, so no more than 16 pages are asked for.
    std::vector<Market::GoodsRow> all;
    all.reserve(16);
    for (std::uint8_t level = 0; level < 16; ++level)
        all.push_back({7181, level, 1000, 1, 1});
    GoodsDirectory sixteen{all, 1};
    CHECK(CountThroughPages(sixteen, 15) == GoodsAvailability{1, true});
    CHECK(sixteen.requests == 16);
    // A list longer than any type can be (not a real answer) stops at the bound and keeps the lower bound.
    const std::vector<Market::GoodsRow> endless(20, {7181, 0, 1000, 1, 1});
    GoodsDirectory broken{endless, 1};
    CHECK_FALSE(CountThroughPages(broken, 5));
    CHECK(broken.requests == MaximumGoodsCountPages);
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

TEST_CASE("Auction capped quotes add one goods-list count per cycle and stay far below the request limit [auction][goods-quote]")
{
    // As above, but every quote shows eight tiers, so the window follows each with the goods-list
    // count (52) through the same single read slot and request clock.
    std::vector<Market::PriceTier> eight;
    eight.reserve(MaximumQuoteTiers);
    for (Market::Zen i = 0; i < MaximumQuoteTiers; ++i)
        eight.push_back({1000000 + 50000 * i, 5});
    QuoteRefresh refresh;
    RequestClock clock;
    const auto start = RequestClock::Time{} + std::chrono::seconds(100);
    refresh.Schedule(start);
    enum class Read : std::uint8_t
    {
        None,
        Quote,
        GoodsCount
    };
    Read queued = Read::None, inFlight = Read::None;
    std::vector<RequestClock::Time> sent;
    int counts = 0;
    RequestClock::Time answerAt{};
    for (int ms = 0; ms <= 120000; ms += 100)
    {
        const auto now = start + std::chrono::milliseconds(ms);
        if (answerAt != RequestClock::Time{} && now >= answerAt)
        {
            answerAt = {};
            if (inFlight == Read::Quote)
            {
                refresh.Schedule(now);
                if (GoodsCountNeeded(AvailableAfterQuote(MakeQuote(1, eight, 1, 1000000))) && queued == Read::None)
                    queued = Read::GoodsCount;
            }
            inFlight = Read::None;
        }
        if (refresh.Due(now, queued != Read::None || inFlight != Read::None))
        {
            refresh.Schedule(now);
            queued = Read::Quote;
        }
        if (queued != Read::None && inFlight == Read::None && clock.Ready(now))
        {
            clock.Sent(now);
            inFlight = std::exchange(queued, Read::None);
            counts += inFlight == Read::GoodsCount;
            sent.push_back(now);
            answerAt = now + std::chrono::milliseconds(200);
        }
    }
    // Two requests per five seconds: 0.4 per second against the server's 5.
    CHECK(counts >= 20);
    CHECK(sent.size() <= 2 * 120 / 5 + 2);
    for (std::size_t i = 0; i < sent.size(); ++i)
    {
        if (i > 0)
            CHECK(sent[i] - sent[i - 1] >= std::chrono::milliseconds(260));
        const auto window = std::count_if(sent.begin(), sent.end(), [&](const auto at)
                                          { return at >= sent[i] && at < sent[i] + std::chrono::seconds(1); });
        CHECK(window <= 2);
    }
}

TEST_CASE("Auction moved lowest price keeps its text line for ten seconds [auction][goods-quote]")
{
    CHECK(LowestMoveText(600000, 590000) == "Lowest fell 600k → 590k");
    CHECK(LowestMoveText(590000, 600000) == "Lowest rose 590k → 600k");
    LowestMove move;
    const auto start = RequestClock::Time{} + std::chrono::seconds(100);
    move.Show(600000, 590000, start);
    CHECK(move.Text() == "Lowest fell 600k → 590k");
    // The row highlight fades after three seconds; the line outlives the next five-second poll.
    CHECK_FALSE(move.Expire(start + std::chrono::seconds(9)));
    CHECK(move.Text() == "Lowest fell 600k → 590k");
    CHECK(move.Expire(start + LowestMove::Lifetime));
    CHECK(move.Text().empty());
    CHECK_FALSE(move.Expire(start + std::chrono::minutes(1)));
    // A selection change or an opened review clears it at once.
    move.Show(590000, 600000, start);
    move.Clear();
    CHECK(move.Text().empty());
    CHECK(MarketDepthLabel(1) == "Market depth (1 price)");
    CHECK(MarketDepthLabel(8) == "Market depth (8 prices)");
}
