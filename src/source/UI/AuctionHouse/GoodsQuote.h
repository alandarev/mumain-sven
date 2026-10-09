#pragma once

#include "GameLogic/AuctionHouse/AuctionHouseTypes.h"
#include "UI/AuctionHouse/RequestClock.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace UI::AuctionHouse
{
namespace Market = GameLogic::AuctionHouse;

// The quote answers at most this many price tiers, cheapest first.
inline constexpr std::size_t MaximumQuoteTiers = 8;

struct OrderTier
{
    Market::Zen unitPrice = 0;
    std::uint32_t stock = 0, inOrder = 0;
};

// How an entered goods quantity fills the quoted tiers, cheapest first. Units filled beyond the
// shown tiers are the quote's fillable quantity and total minus the visible part.
struct OrderSplit
{
    std::vector<OrderTier> tiers;
    std::uint32_t requested = 0, fillable = 0, beyondQuantity = 0;
    Market::Zen total = 0;
    std::uint64_t beyondTotal = 0;
};

OrderSplit SplitOrder(const Market::Quote& quote);
// "6 × 590,000 = 3,540,000" per filled tier, then "+N at higher prices · X Zen" when needed.
std::vector<std::string> OrderSplitLines(const OrderSplit& split);
// "8 for 4,730,000 Zen"; empty when nothing is fillable.
std::string OrderSummaryText(const OrderSplit& split);
// "+N at higher prices · X Zen"; empty when the shown tiers cover the order.
std::string BeyondTiersText(const OrderSplit& split);
// "Only N available"; empty when the whole order is fillable.
std::string ShortfallText(const OrderSplit& split);
// The units of other sellers for sale: exact, or a lower bound ("40+ available") that is no ceiling
// for the quantity (CR-04).
struct GoodsAvailability
{
    std::uint32_t count = 0;
    bool exact = true;

    bool operator==(const GoodsAvailability&) const = default;
};

// After a quote. A shortfall answer counts them exactly; so do fewer than MaximumQuoteTiers tiers
// (each tier carries its whole stock). A full tier list is only a lower bound: the window then asks
// the goods list (52) of that type for the exact count (GoodsCountNeeded).
GoodsAvailability AvailableAfterQuote(const Market::Quote& quote);
bool GoodsCountNeeded(const GoodsAvailability& availability);
// The goods list asked for one type holds one row per level, ordered by level, in pages of the
// server's PageSize; a type has at most this many levels, so this many pages always reach the
// selected one.
inline constexpr std::uint16_t MaximumGoodsCountPages = 16;

// One answered page of that list: its rows, the list's TotalCount and the rows of earlier pages.
struct GoodsCountPage
{
    std::span<const Market::GoodsRow> rows;
    std::uint32_t totalCount = 0, loadedBefore = 0;
};

// The exact count of the selected goods from one page: its row, or none for sale once the list
// holds no more rows or has passed the selected level. Empty while the selected level may still be
// on a later page (RR-02): the count stays a lower bound and the next page is asked for.
std::optional<GoodsAvailability> AvailableFromGoodsList(const GoodsCountPage& page, std::uint16_t type,
                                                        std::uint8_t level);

// The window's exact count of the selected goods: which goods-list page to ask for next, and what
// an answered page means (AvailableFromGoodsList).
class GoodsCountPaging
{
public:
    void Start(std::uint16_t type, std::uint8_t level)
    {
        *this = {};
        m_Type = type;
        m_Level = level;
    }
    std::uint16_t Type() const
    {
        return m_Type;
    }
    std::uint8_t Level() const
    {
        return m_Level;
    }
    std::uint16_t Page() const
    {
        return m_Page;
    }
    // The exact count, or empty; then More() tells whether the next page is worth asking for.
    std::optional<GoodsAvailability> Apply(std::span<const Market::GoodsRow> rows, std::uint32_t totalCount);
    bool More() const
    {
        return m_More;
    }

private:
    std::uint16_t m_Type = 0, m_Page = 0;
    std::uint8_t m_Level = 0;
    std::uint32_t m_Loaded = 0;
    bool m_More = false;
};

// The selling context line: "Lowest 600,000 · 20 in stock · 50 total". The stock is the stock at
// the lowest price; the total covers every quoted tier ("+" when the tier list is full).
std::string MarketLowestText(Market::Zen lowest, std::span<const Market::PriceTier> tiers);
// The collapsed seller depth: "Market depth (2 prices)".
std::string MarketDepthLabel(std::size_t tierCount);

struct TierChange
{
    bool added = false, stockChanged = false;
    std::uint32_t previousStock = 0;
};

// One entry per tier of `after`: a price that was not quoted before, or a changed stock.
std::vector<TierChange> CompareTiers(std::span<const Market::PriceTier> before,
                                     std::span<const Market::PriceTier> after);
bool AnyTierChange(std::span<const TierChange> changes);
std::string StockChangeText(std::uint32_t before, std::uint32_t after);
std::string PriceChangeText(Market::Zen before, Market::Zen after);
std::string QuoteAgeText(std::chrono::seconds age);
// "Lowest fell 600k → 590k" or "Lowest rose 590k → 600k".
std::string LowestMoveText(Market::Zen before, Market::Zen after);

// The text line of a moved lowest price (D23). The row highlight fades after a few seconds; this
// line stays for Lifetime, or until the selection changes or a review opens (Clear).
class LowestMove
{
public:
    using Time = RequestClock::Time;
    static constexpr std::chrono::seconds Lifetime{10};

    void Show(Market::Zen before, Market::Zen after, Time now);
    void Clear();
    // Clears an expired line; returns whether it did.
    bool Expire(Time now);
    const std::string& Text() const
    {
        return m_Text;
    }

private:
    std::string m_Text;
    Time m_Until{};
};

// The live quote of the selected goods row: due every Interval after the last request or answer,
// never while paused (a review, a request in flight, or no selection).
class QuoteRefresh
{
public:
    using Time = RequestClock::Time;
    static constexpr std::chrono::seconds Interval{5};

    void Schedule(Time now)
    {
        m_Next = now + Interval;
        m_Active = true;
    }
    void Stop()
    {
        m_Active = false;
    }
    bool Active() const
    {
        return m_Active;
    }
    bool Due(Time now, bool paused) const
    {
        return m_Active && !paused && now >= m_Next;
    }

private:
    Time m_Next{};
    bool m_Active = false;
};
} // namespace UI::AuctionHouse
