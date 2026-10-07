#include "GameLogic/AuctionHouse/AuctionHouseWire.h"

namespace GameLogic::AuctionHouse::Wire
{
std::optional<ServerPacket> ParseOwnListings(Reader& in)
{
    OwnListingsPage page;
    static_cast<Reply&>(page) = in.ReplyHeader();
    const auto count = in.Get<std::uint8_t>();
    if (page.result != Result::Success && count != 0)
        return std::nullopt;
    for (std::size_t i = 0; i < count; ++i)
    {
        OwnListing row;
        row.id = in.Id();
        row.unitPrice = in.Get<Zen>();
        row.listed = in.Get<std::uint32_t>();
        row.remaining = in.Get<std::uint32_t>();
        row.secondsLeft = in.Get<std::uint32_t>();
        row.kind = in.Get<std::uint8_t>();
        row.type = in.Get<std::uint16_t>();
        row.level = in.Get<std::uint8_t>();
        row.itemData = in.Item(row.kind == 1);
        if (row.kind < 1 || row.kind > 2 || (row.kind == 2 && !row.itemData.empty()))
            in.Reject();
        page.rows.push_back(std::move(row));
    }
    return Finish(in, std::move(page));
}

std::optional<ServerPacket> ParseCollection(Reader& in)
{
    CollectionPage page;
    static_cast<Reply&>(page) = in.ReplyHeader();
    page.page = in.Get<std::uint16_t>();
    page.totalCount = in.Get<std::uint16_t>();
    const auto count = in.Get<std::uint8_t>();
    if (page.result != Result::Success && count != 0)
        return std::nullopt;
    for (std::size_t i = 0; i < count; ++i)
    {
        CollectionEntry row;
        row.id = in.Id();
        row.kind = in.Get<std::uint8_t>();
        row.type = in.Get<std::uint16_t>();
        row.level = in.Get<std::uint8_t>();
        row.quantity = in.Get<std::uint32_t>();
        row.gross = in.Get<Zen>();
        row.fee = in.Get<Zen>();
        row.net = in.Get<Zen>();
        row.itemData = in.Item(false);
        if (row.kind < 1 || row.kind > 4)
            in.Reject();
        page.rows.push_back(std::move(row));
    }
    return Finish(in, std::move(page));
}

std::optional<ServerPacket> ParseSellCheck(Reader& in)
{
    SellCheck check;
    static_cast<Reply&>(check) = in.ReplyHeader();
    check.inventorySlot = in.Get<std::uint8_t>();
    check.reason = in.Get<std::uint8_t>();
    check.kind = in.Get<std::uint8_t>();
    check.type = in.Get<std::uint16_t>();
    check.level = in.Get<std::uint8_t>();
    check.available = in.Get<std::uint32_t>();
    check.lowestUnitPrice = in.Get<Zen>();
    check.activeListings = in.Get<std::uint8_t>();
    return Finish(in, check);
}

std::optional<ServerPacket> ParseNotice(Reader& in)
{
    Notice notice;
    notice.kind = static_cast<NoticeKind>(in.Get<std::uint8_t>());
    notice.type = in.Get<std::uint16_t>();
    notice.level = in.Get<std::uint8_t>();
    notice.quantity = in.Get<std::uint32_t>();
    notice.net = in.Get<Zen>();
    notice.pendingCollection = in.Get<std::uint16_t>();
    if (notice.kind < NoticeKind::SoldToCollection || notice.kind > NoticeKind::ExpiredToInventory)
        return std::nullopt;
    return Finish(in, notice);
}
} // namespace GameLogic::AuctionHouse::Wire
