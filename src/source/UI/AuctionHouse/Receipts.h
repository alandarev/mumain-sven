#pragma once

#include "GameLogic/AuctionHouse/AuctionHouseTypes.h"
#include "UI/AuctionHouse/RequestClock.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

namespace UI::AuctionHouse
{
namespace Market = GameLogic::AuctionHouse;

inline constexpr std::string_view OutcomeUnknownText =
    "Outcome unknown — reopen the broker and check your bag and My auctions.";

enum class StatusTone : std::uint8_t
{
    Info,
    Success,
    Error
};

// Where a result line offers to go next.
enum class StatusLink : std::uint8_t
{
    None,
    Collection,
    OwnListings
};

std::string_view StatusLinkName(StatusLink link);

// The window's one status line. A success or notice expires after Lifetime; an error stays until it
// is replaced or cleared by the player's next action. Showing a new line cancels the old expiry.
class StatusLine
{
public:
    using Time = RequestClock::Time;
    static constexpr std::chrono::seconds Lifetime{8};

    void Show(std::string text, StatusTone tone, StatusLink link, Time now);
    void Clear();
    // Clears an expired line; returns whether it did.
    bool Expire(Time now);
    const std::string& Text() const
    {
        return m_Text;
    }
    StatusTone Tone() const
    {
        return m_Tone;
    }
    StatusLink Link() const
    {
        return m_Link;
    }

private:
    std::string m_Text;
    StatusTone m_Tone = StatusTone::Info;
    StatusLink m_Link = StatusLink::None;
    Time m_Until{};
};

// What the player acted on, as reviewed; the outcome carries what the server actually did.
struct ReceiptSubject
{
    std::string name;
    Market::Zen unitPrice = 0;
};

struct Receipt
{
    std::string text;
    StatusTone tone = StatusTone::Success;
    StatusLink link = StatusLink::None;
    // New Collection entries the action created; the badge counts entries.
    std::uint16_t collectionEntries = 0;
};

// Built from the result fields of 54 BuyListing, 55 BuyGoods, 56 CreateListing, 57 CancelListing
// and 5A Claim (Success, or Partial for a claim).
Receipt BuildReceipt(const Market::Outcome& outcome, const ReceiptSubject& subject);

// "5 %", "2.5 %", "0.25 %".
std::string PercentText(std::uint16_t basisPoints);

struct ListingReviewText
{
    std::string total, fees;
};

// The listing review: the unit price line for goods, the posting fee with its terms, the sale fee,
// what the seller receives if everything sells, and the duration.
ListingReviewText DescribeListingReview(const Market::Rules& rules, bool goods, std::uint32_t quantity,
                                        Market::Zen unitPrice, Market::Zen postingFee, std::uint8_t durationHours);

struct FieldHint
{
    std::string text;
    bool error = false;
};

// "8 available", or "Whole number 1–8" while the entered quantity is outside that range.
FieldHint QuantityHint(std::string_view text, std::uint32_t available);
// "Need 177,000 Zen, you have 50,000".
std::string ZenShortfallText(std::uint64_t need, std::uint64_t have);
// "Listings 19/20".
std::string ListingCountText(std::uint8_t active, std::uint8_t maximum);
// The sell check's Reason (1 not tradable, 2 no item in the slot, 3 not accepted by the market).
std::string SellCheckReasonText(std::uint8_t reason);
} // namespace UI::AuctionHouse
