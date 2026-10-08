#include "GameLogic/AuctionHouse/AuctionHouseWire.h"

#include <limits>

namespace GameLogic::AuctionHouse::Wire
{
std::optional<ServerPacket> ParseRules(Reader& in)
{
    RulesUpdate update;
    const auto state = in.Get<std::uint8_t>();
    update.open = state == 1;
    auto& rules = update.rules;
    rules.pageSize = in.Get<std::uint8_t>();
    rules.maxActiveListings = in.Get<std::uint8_t>();
    rules.activeListings = in.Get<std::uint8_t>();
    rules.pendingCollection = in.Get<std::uint16_t>();
    rules.zenLimit = in.Get<Zen>();
    rules.maxPrice = in.Get<Zen>();
    rules.postingFeeFlat = in.Get<Zen>();
    rules.postingFeeBasisPoints = in.Get<std::uint16_t>();
    rules.saleFeeBasisPoints = in.Get<std::uint16_t>();
    const auto flags = in.Get<std::uint8_t>();
    rules.refundPostingFee = (flags & 1) != 0;
    const auto count = in.Get<std::uint8_t>();
    const auto durations = in.Take(4);
    rules.priceStepPrecision = in.Get<std::uint8_t>();
    if (state > 1 || count > 4 || durations.size() != 4 || flags > 1)
        return std::nullopt;
    if (update.open && (count == 0 || rules.pageSize == 0 || rules.maxActiveListings == 0 || rules.maxPrice == 0 ||
                        rules.maxPrice > rules.zenLimit ||
                        rules.zenLimit > static_cast<Zen>(std::numeric_limits<std::int32_t>::max())))
        return std::nullopt;
    for (std::size_t i = 0; i < durations.size(); ++i)
    {
        if ((i < count) == (durations[i] == 0))
            return std::nullopt;
        if (i < count)
            rules.durations.push_back(durations[i]);
    }
    if (!update.open)
        update.rules = {};
    return Finish(in, std::move(update));
}

std::optional<ServerPacket> ParseSearch(Reader& in)
{
    SearchPage page;
    static_cast<Reply&>(page) = in.ReplyHeader();
    page.page = in.Get<std::uint16_t>();
    page.totalCount = in.Get<std::uint32_t>();
    const auto count = in.Get<std::uint8_t>();
    if (page.result != Result::Success && count != 0)
        return std::nullopt;
    for (std::size_t i = 0; i < count; ++i)
    {
        ListingRow row;
        row.id = in.Id();
        row.price = in.Get<Zen>();
        row.secondsLeft = in.Get<std::uint32_t>();
        const auto name = in.Take(10);
        const auto end = std::find(name.begin(), name.end(), std::uint8_t{0});
        row.seller.assign(name.begin(), end);
        const auto flags = in.Get<std::uint8_t>();
        row.requirementsMet = (flags & 1) != 0;
        row.ownListing = (flags & 2) != 0;
        if (flags > 3)
            in.Reject();
        row.itemData = in.Item(true);
        page.rows.push_back(std::move(row));
    }
    return Finish(in, std::move(page));
}

std::optional<ServerPacket> ParseGoods(Reader& in)
{
    GoodsPage page;
    static_cast<Reply&>(page) = in.ReplyHeader();
    page.page = in.Get<std::uint16_t>();
    page.totalCount = in.Get<std::uint32_t>();
    const auto count = in.Get<std::uint8_t>();
    if (page.result != Result::Success && count != 0)
        return std::nullopt;
    for (std::size_t i = 0; i < count; ++i)
    {
        GoodsRow row;
        row.type = in.Get<std::uint16_t>();
        row.level = in.Get<std::uint8_t>();
        row.lowestUnitPrice = in.Get<Zen>();
        row.available = in.Get<std::uint32_t>();
        row.listingCount = in.Get<std::uint16_t>();
        page.rows.push_back(row);
    }
    return Finish(in, std::move(page));
}

std::optional<ServerPacket> ParseQuote(Reader& in)
{
    Quote quote;
    static_cast<Reply&>(quote) = in.ReplyHeader();
    quote.type = in.Get<std::uint16_t>();
    quote.level = in.Get<std::uint8_t>();
    quote.requestedQuantity = in.Get<std::uint32_t>();
    quote.fillableQuantity = in.Get<std::uint32_t>();
    quote.total = in.Get<Zen>();
    const auto count = in.Get<std::uint8_t>();
    if (count > 8 || quote.fillableQuantity > quote.requestedQuantity)
        return std::nullopt;
    for (std::size_t i = 0; i < count; ++i)
    {
        PriceTier tier{in.Get<Zen>(), in.Get<std::uint32_t>()};
        if (tier.unitPrice == 0 || tier.quantity == 0 ||
            (!quote.tiers.empty() && quote.tiers.back().unitPrice > tier.unitPrice))
            in.Reject();
        quote.tiers.push_back(tier);
    }
    return Finish(in, std::move(quote));
}
} // namespace GameLogic::AuctionHouse::Wire
