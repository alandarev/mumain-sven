#pragma once

#include "GameLogic/AuctionHouse/AuctionHouse.h"
#include "GameLogic/AuctionHouse/AuctionHousePacketFormat.h"

namespace GameLogic::AuctionHouse::Detail
{
class Controller
{
public:
    const State& GetState() const
    {
        return state;
    }
    void SetListener(IListener* value)
    {
        listener = value;
    }
    std::uint16_t NextId();
    bool Send(Operation op, PacketBytes packet);
    bool Abandon(Operation op);
    void Receive(std::span<const std::uint8_t> packet);
    void Reset();

private:
    bool Accept(Operation op, const Reply& reply, bool replacementQuote = false);
    void Changed(Operation op);
    void Apply(RulesUpdate value);
    void Apply(SearchPage value);
    void Apply(GoodsPage value);
    void Apply(Quote value);
    void Apply(SellCheck value);
    void Apply(OwnListingsPage value);
    void Apply(CollectionPage value);
    void Apply(Outcome value);
    void Apply(Notice value);

    State state;
    IListener* listener = nullptr;
    std::uint16_t nextId = 0;
    bool awaitingPurchaseQuote = false;
    std::uint16_t purchaseQuoteId = 0;
};
} // namespace GameLogic::AuctionHouse::Detail
