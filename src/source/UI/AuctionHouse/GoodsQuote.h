#pragma once

#include "GameLogic/AuctionHouse/AuctionHouseTypes.h"
#include "UI/AuctionHouse/RequestClock.h"

#include <chrono>
#include <cstdint>
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

// The selling context line: "Lowest 600,000 · 20 in stock · 50 total". The stock is the stock at
// the lowest price; the total covers every quoted tier ("+" when the tier list is full).
std::string MarketLowestText(Market::Zen lowest, std::span<const Market::PriceTier> tiers);

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
