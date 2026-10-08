#include "GameLogic/AuctionHouse/AuctionHouseController.h"
#include "GameLogic/AuctionHouse/AuctionHouseTransport.h"

#include <algorithm>

namespace GameLogic::AuctionHouse
{
namespace
{
Detail::Controller controller;
bool PriceValid(Zen price)
{
    return price != 0 && price <= GetState().GetRules().maxPrice;
}
bool HasId(const EntityId& id)
{
    return std::any_of(id.begin(), id.end(), [](auto byte) { return byte != 0; });
}
bool GoodsValid(std::uint16_t type, std::uint8_t level, std::uint32_t quantity)
{
    return type < 16 * 512 && level <= 15 && quantity != 0;
}
} // namespace

const State& GetState()
{
    return controller.GetState();
}
void SetListener(IListener* listener)
{
    controller.SetListener(listener);
}
void ReceivePacket(std::span<const std::uint8_t> packet)
{
    controller.Receive(packet);
}
void Reset()
{
    controller.Reset();
}

bool RequestSearch(const SearchFilter& filter, std::uint16_t page)
{
    if (filter.maxPrice > GetState().GetRules().zenLimit)
        return false;
    return controller.Send(Operation::Search, BuildSearchRequest(controller.NextId(), filter, page));
}

bool RequestGoodsList(std::span<const std::uint16_t> types, std::uint16_t page)
{
    return controller.Send(Operation::GoodsList, BuildGoodsListRequest(controller.NextId(), types, page));
}

bool RequestQuote(std::uint16_t type, std::uint8_t level, std::uint32_t quantity)
{
    if (!GoodsValid(type, level, quantity))
        return false;
    return controller.Send(Operation::Quote, BuildQuoteRequest(controller.NextId(), type, level, quantity));
}

bool BuyListing(const EntityId& id, Zen price)
{
    if (!HasId(id) || !PriceValid(price))
        return false;
    return controller.Send(Operation::BuyListing, BuildBuyListingRequest(controller.NextId(), id, price));
}

bool BuyGoods(std::uint16_t type, std::uint8_t level, std::uint32_t quantity, Zen total)
{
    if (!GoodsValid(type, level, quantity) || !PriceValid(total))
        return false;
    return controller.Send(Operation::BuyGoods,
                           BuildBuyGoodsRequest(controller.NextId(), type, level, quantity, total));
}

bool CheckSell(std::uint8_t slot)
{
    return controller.Send(Operation::SellCheck, BuildSellCheckRequest(controller.NextId(), slot));
}

bool CreateListing(std::uint8_t slot, std::uint32_t quantity, Zen price, std::uint8_t duration, Zen fee)
{
    const auto& rules = GetState().GetRules();
    if (!PriceOnStep(rules, price))
        return false;
    const auto expected = PostingFee(rules, quantity, price);
    if (!expected || *expected != fee ||
        std::find(rules.durations.begin(), rules.durations.end(), duration) == rules.durations.end())
        return false;
    return controller.Send(Operation::CreateListing,
                           BuildCreateListingRequest(controller.NextId(), slot, quantity, price, duration, fee));
}

bool CancelListing(const EntityId& id)
{
    if (!HasId(id))
        return false;
    return controller.Send(Operation::CancelListing, BuildCancelListingRequest(controller.NextId(), id));
}

bool RequestOwnListings()
{
    return controller.Send(Operation::OwnListings, BuildOwnListingsRequest(controller.NextId()));
}

bool RequestCollection(std::uint16_t page)
{
    return controller.Send(Operation::Collection, BuildCollectionRequest(controller.NextId(), page));
}

bool Claim(const EntityId& id)
{
    return controller.Send(Operation::Claim, BuildClaimRequest(controller.NextId(), id));
}

void Close()
{
    const bool wasOpen = GetState().IsOpen();
    Reset();
    if (wasOpen)
        Transport::CloseDialog();
}
} // namespace GameLogic::AuctionHouse
