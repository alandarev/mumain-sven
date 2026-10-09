#include "UI/AuctionHouse/GoodsQuote.h"

#include "UI/AuctionHouse/Presentation.h"

#include <algorithm>

namespace UI::AuctionHouse
{
OrderSplit SplitOrder(const Market::Quote& quote)
{
    OrderSplit split;
    split.requested = quote.requestedQuantity;
    split.fillable = std::min(quote.fillableQuantity, quote.requestedQuantity);
    split.total = quote.total;
    std::uint32_t left = split.fillable, shown = 0;
    std::uint64_t shownTotal = 0;
    for (const auto& tier : quote.tiers)
    {
        const auto take = std::min(left, tier.quantity);
        split.tiers.push_back({tier.unitPrice, tier.quantity, take});
        left -= take;
        shown += take;
        shownTotal += static_cast<std::uint64_t>(take) * tier.unitPrice;
    }
    split.beyondQuantity = split.fillable - shown;
    split.beyondTotal = split.total > shownTotal ? split.total - shownTotal : 0;
    return split;
}

std::vector<std::string> OrderSplitLines(const OrderSplit& split)
{
    std::vector<std::string> lines;
    for (const auto& tier : split.tiers)
        if (tier.inOrder)
            lines.push_back(std::to_string(tier.inOrder) + " × " + ExactZen(tier.unitPrice) + " = " +
                            ExactZen(static_cast<std::uint64_t>(tier.inOrder) * tier.unitPrice));
    if (auto beyond = BeyondTiersText(split); !beyond.empty())
        lines.push_back(std::move(beyond));
    return lines;
}

std::string OrderSummaryText(const OrderSplit& split)
{
    if (!split.fillable)
        return {};
    return std::to_string(split.fillable) + " for " + ExactZen(split.total) + " Zen";
}

std::string BeyondTiersText(const OrderSplit& split)
{
    if (!split.beyondQuantity)
        return {};
    return "+" + std::to_string(split.beyondQuantity) + " at higher prices · " + ExactZen(split.beyondTotal) + " Zen";
}

std::string ShortfallText(const OrderSplit& split)
{
    if (split.fillable >= split.requested)
        return {};
    return "Only " + std::to_string(split.fillable) + " available";
}

GoodsAvailability AvailableAfterQuote(const Market::Quote& quote)
{
    if (quote.fillableQuantity < quote.requestedQuantity)
        return {quote.fillableQuantity, true};
    std::uint64_t stock = 0;
    for (const auto& tier : quote.tiers)
        stock += tier.quantity;
    const auto counted = static_cast<std::uint32_t>(std::min<std::uint64_t>(stock, UINT32_MAX));
    if (quote.tiers.size() < MaximumQuoteTiers)
        return {counted, true};
    return {std::max(counted, quote.fillableQuantity), false};
}

bool GoodsCountNeeded(const GoodsAvailability& availability)
{
    return !availability.exact;
}

std::optional<GoodsAvailability> AvailableFromGoodsList(const GoodsCountPage& page, std::uint16_t type,
                                                        std::uint8_t level)
{
    const auto& rows = page.rows;
    const auto row = std::find_if(rows.begin(), rows.end(), [type, level](const auto& goods)
                                  { return goods.type == type && goods.level == level; });
    if (row != rows.end())
        return GoodsAvailability{row->available, true};
    const bool exhausted = rows.empty() || std::uint64_t{page.loadedBefore} + rows.size() >= page.totalCount;
    const bool passed =
        !rows.empty() && (rows.back().type > type || (rows.back().type == type && rows.back().level > level));
    if (exhausted || passed)
        return GoodsAvailability{0, true};
    return std::nullopt;
}

std::optional<GoodsAvailability> GoodsCountPaging::Apply(std::span<const Market::GoodsRow> rows,
                                                         std::uint32_t totalCount)
{
    m_More = false;
    const auto exact = AvailableFromGoodsList({rows, totalCount, m_Loaded}, m_Type, m_Level);
    if (exact || m_Page + 1 >= MaximumGoodsCountPages)
        return exact;
    m_Loaded += static_cast<std::uint32_t>(rows.size());
    ++m_Page;
    m_More = true;
    return std::nullopt;
}

std::string MarketLowestText(Market::Zen lowest, std::span<const Market::PriceTier> tiers)
{
    if (lowest == 0)
        return "No other sellers";
    auto text = "Lowest " + ExactZen(lowest);
    std::uint64_t atLowest = 0, total = 0;
    for (const auto& tier : tiers)
    {
        total += tier.quantity;
        if (tier.unitPrice == lowest)
            atLowest += tier.quantity;
    }
    // A quote from before the lowest price moved says nothing about the stock at it.
    if (!atLowest)
        return text;
    text += " · " + std::to_string(atLowest) + " in stock";
    if (total > atLowest)
        text += " · " + std::to_string(total) + (tiers.size() >= MaximumQuoteTiers ? "+" : "") + " total";
    return text;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
std::vector<TierChange> CompareTiers(std::span<const Market::PriceTier> before,
                                     std::span<const Market::PriceTier> after)
{
    std::vector<TierChange> changes;
    changes.reserve(after.size());
    for (const auto& tier : after)
    {
        const auto old = std::find_if(before.begin(), before.end(),
                                      [&tier](const auto& previous) { return previous.unitPrice == tier.unitPrice; });
        TierChange change;
        if (old == before.end())
            change.added = true;
        else if (old->quantity != tier.quantity)
        {
            change.stockChanged = true;
            change.previousStock = old->quantity;
        }
        changes.push_back(change);
    }
    return changes;
}

bool AnyTierChange(std::span<const TierChange> changes)
{
    return std::any_of(changes.begin(), changes.end(),
                       [](const auto& change) { return change.added || change.stockChanged; });
}

std::string StockChangeText(std::uint32_t before, std::uint32_t after)
{
    return std::to_string(before) + " → " + std::to_string(after);
}

std::string PriceChangeText(Market::Zen before, Market::Zen after)
{
    return CompactZen(before) + " → " + CompactZen(after);
}

std::string MarketDepthLabel(std::size_t tierCount)
{
    return "Market depth (" + std::to_string(tierCount) + (tierCount == 1 ? " price)" : " prices)");
}

std::string LowestMoveText(Market::Zen before, Market::Zen after)
{
    return std::string(after < before ? "Lowest fell " : "Lowest rose ") + PriceChangeText(before, after);
}

void LowestMove::Show(Market::Zen before, Market::Zen after, Time now)
{
    m_Text = LowestMoveText(before, after);
    m_Until = now + Lifetime;
}

void LowestMove::Clear()
{
    m_Text.clear();
    m_Until = {};
}

bool LowestMove::Expire(Time now)
{
    if (m_Text.empty() || now < m_Until)
        return false;
    Clear();
    return true;
}

std::string QuoteAgeText(std::chrono::seconds age)
{
    const auto seconds = std::max<long long>(0, age.count());
    if (seconds < 2)
        return "Updated just now";
    if (seconds < 60)
        return "Updated " + std::to_string(seconds) + " s ago";
    return "Updated " + std::to_string(seconds / 60) + " min ago";
}
} // namespace UI::AuctionHouse
