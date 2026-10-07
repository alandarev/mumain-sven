#include "GameLogic/AuctionHouse/AuctionHouseWire.h"

namespace GameLogic::AuctionHouse::Wire
{
namespace
{
void BuyListing(Reader& in, Outcome& out)
{
    out.operation = Operation::BuyListing;
    out.id = in.Id();
    out.paid = in.Get<Zen>();
    out.delivery = in.Get<std::uint8_t>();
    if (out.delivery > 2)
        in.Reject();
}

void BuyGoods(Reader& in, Outcome& out)
{
    out.operation = Operation::BuyGoods;
    out.type = in.Get<std::uint16_t>();
    out.level = in.Get<std::uint8_t>();
    out.quantity = in.Get<std::uint32_t>();
    out.paid = in.Get<Zen>();
    out.delivered = in.Get<std::uint32_t>();
    out.kept = in.Get<std::uint32_t>();
}

void CreateListing(Reader& in, Outcome& out)
{
    out.operation = Operation::CreateListing;
    out.id = in.Id();
    out.quantity = in.Get<std::uint32_t>();
    out.postingFee = in.Get<Zen>();
}

void CancelListing(Reader& in, Outcome& out)
{
    out.operation = Operation::CancelListing;
    out.id = in.Id();
    out.delivered = in.Get<std::uint32_t>();
    out.kept = in.Get<std::uint32_t>();
    out.refundedFee = in.Get<Zen>();
}

void Claim(Reader& in, Outcome& out)
{
    out.operation = Operation::Claim;
    out.completedEntries = in.Get<std::uint16_t>();
    out.itemsMoved = in.Get<std::uint32_t>();
    out.zenCollected = in.Get<Zen>();
    out.remainingEntries = in.Get<std::uint16_t>();
    out.keptBack = in.Get<std::uint8_t>();
    if (out.keptBack > 3)
        in.Reject();
}
} // namespace

std::optional<ServerPacket> ParseOutcome(Reader& in, std::uint8_t subcode)
{
    Outcome outcome;
    static_cast<Reply&>(outcome) = in.ReplyHeader();
    switch (subcode)
    {
    case 0x54:
        BuyListing(in, outcome);
        break;
    case 0x55:
        BuyGoods(in, outcome);
        break;
    case 0x56:
        CreateListing(in, outcome);
        break;
    case 0x57:
        CancelListing(in, outcome);
        break;
    case 0x5A:
        Claim(in, outcome);
        break;
    default:
        return std::nullopt;
    }
    return Finish(in, outcome);
}
} // namespace GameLogic::AuctionHouse::Wire
