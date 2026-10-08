#pragma once

#include "GameLogic/AuctionHouse/AuctionHouseTypes.h"

#include <optional>
#include <span>
#include <string_view>

namespace GameLogic::AuctionHouse
{
namespace Detail
{
class Controller;
}

class IListener
{
public:
    virtual ~IListener() = default;
    virtual void OnOpened() = 0;
    virtual void OnClosed() = 0;
    virtual void OnChanged(Operation topic) = 0;
    virtual void OnNotice(const Notice& notice) = 0;
};

class State
{
public:
    bool IsOpen() const
    {
        return open;
    }
    const Rules& GetRules() const
    {
        return rules;
    }
    std::uint32_t Revision() const
    {
        return revision;
    }
    const SearchPage& Search() const
    {
        return search;
    }
    const GoodsPage& Goods() const
    {
        return goods;
    }
    const std::optional<Quote>& CurrentQuote() const
    {
        return quote;
    }
    const std::optional<SellCheck>& LastSellCheck() const
    {
        return sellCheck;
    }
    std::span<const OwnListing> OwnListings() const
    {
        return ownListings.rows;
    }
    ReceivedTime OwnListingsReceivedAt() const
    {
        return ownListings.receivedAt;
    }
    const CollectionPage& Collection() const
    {
        return collection;
    }
    std::span<const Notice> Notices() const
    {
        return notices;
    }
    bool IsPending(Operation op) const;
    std::optional<Outcome> LastOutcome(Operation op) const;

private:
    friend class Detail::Controller;
    struct RequestState
    {
        std::uint16_t id = 0;
        bool pending = false;
        std::optional<Outcome> outcome;
    };
    bool open = false;
    Rules rules;
    std::uint32_t revision = 0;
    SearchPage search;
    GoodsPage goods;
    std::optional<Quote> quote;
    std::optional<SellCheck> sellCheck;
    OwnListingsPage ownListings;
    CollectionPage collection;
    std::vector<Notice> notices;
    std::array<RequestState, static_cast<std::size_t>(Operation::Count)> requests{};
};

const State& GetState();
void SetListener(IListener* listener);
bool RequestSearch(const SearchFilter&, std::uint16_t page);
bool RequestGoodsList(std::span<const std::uint16_t> types, std::uint16_t page);
bool RequestQuote(std::uint16_t type, std::uint8_t level, std::uint32_t quantity);
bool BuyListing(const EntityId&, Zen reviewedPrice);
bool BuyGoods(std::uint16_t type, std::uint8_t level, std::uint32_t quantity, Zen reviewedTotal);
bool CheckSell(std::uint8_t slot);
bool CreateListing(std::uint8_t slot, std::uint32_t quantity, Zen unitPrice, std::uint8_t durationHours,
                   Zen expectedPostingFee);
bool CancelListing(const EntityId&);
bool RequestOwnListings();
bool RequestCollection(std::uint16_t page);
bool Claim(const EntityId&);
void Close();
void ReceivePacket(std::span<const std::uint8_t>);
void Reset();

std::optional<Zen> PostingFee(const Rules&, std::uint32_t quantity, Zen unitPrice);
Zen SaleFee(const Rules&, Zen gross);
bool PriceOnStep(const Rules&, Zen price);
std::optional<Zen> RoundPriceDown(const Rules&, Zen price);
std::optional<Zen> PreviousPrice(const Rules&, Zen price);
std::optional<Zen> NextPrice(const Rules&, Zen price);
// Moves up to `steps` valid prices; stops early at 1 or MaxPrice and is empty when no step is possible.
std::optional<Zen> StepPrice(const Rules&, Zen price, bool up, unsigned steps);
std::uint32_t SecondsRemaining(std::uint32_t secondsLeft, ReceivedTime receivedAt, ReceivedTime now);
enum class NameMatch : std::uint8_t
{
    Ok,
    NoMatch,
    TooBroad
};
NameMatch ResolveItemTypes(std::wstring_view text, std::uint16_t slotMask, std::uint16_t groupMask,
                           std::vector<std::uint16_t>& out);
// The ancient sets with a member in the slot and group domain (0 = any) wearable by the class line
// (`custom-packets.md`, ClassLine; 0xFF = any), sorted by name.
std::vector<std::wstring> AncientSets(std::uint16_t slotMask = 0, std::uint16_t groupMask = 0,
                                      std::uint8_t classLine = 0xFF);
// The item type's slot bit in the search request's SlotMask, or 0 for items without an equipment slot.
std::uint16_t ItemSlotMask(int type);
// Whether any item type in the slot and group domain can carry a skill, so a Skill filter can match.
bool SkillApplies(std::uint16_t slotMask, std::uint16_t groupMask);
NameMatch ResolveAncientSet(std::wstring_view name, std::uint16_t slots, std::uint16_t groups,
                            std::vector<SearchEntry>& out);
} // namespace GameLogic::AuctionHouse
