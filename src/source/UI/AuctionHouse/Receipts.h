#pragma once

#include "GameLogic/AuctionHouse/AuctionHouseTypes.h"
#include "UI/AuctionHouse/GoodsQuote.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace UI::AuctionHouse
{
namespace Market = GameLogic::AuctionHouse;

inline constexpr std::string_view OutcomeUnknownText =
    "Outcome unknown. Reopen Zyro and check your bag, Collection and My auctions before retrying.";

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

// The window's one result line: a receipt, an error or an info line. It never fades by itself, so its
// link stays usable; the player's next deliberate action in the window or its × clears it (D18).
class StatusLine
{
public:
    void Show(std::string text, StatusTone tone, StatusLink link);
    void Clear();
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
};

struct NoticeLine
{
    std::string text;
    StatusLink link = StatusLink::None;
    // A server-wide announcement drawn here instead of over the window (D24).
    bool announcement = false;
};

// Sale and expiry notices from the server (D19): their own lines above the result line, newest
// first, at most MaximumLines. Above them one slot for a server announcement (D24), where the newest
// replaces an older one; the two never evict each other (RR-03). Like a receipt they stay until the
// next deliberate action or their ×.
class NoticeLines
{
public:
    static constexpr std::size_t MaximumLines = 3;

    void Push(NoticeLine line);
    void PushAnnouncement(std::string text);
    // `index` into Lines().
    void Dismiss(std::size_t index);
    void Clear();
    // The announcement, if any, then the sale notices.
    std::span<const NoticeLine> Lines() const
    {
        return m_Lines;
    }

private:
    void Compose();

    std::optional<NoticeLine> m_Announcement;
    std::vector<NoticeLine> m_Sales, m_Lines;
};

// "Sold 6 × Jewel of Bless · 3,186,000 Zen added to your bag", "Expired: Dragon Armor +9 · returned to
// your Collection", "2 deliveries waiting in your Collection"; built from the 5B notice fields.
NoticeLine DescribeNotice(const Market::Notice& notice, std::string_view itemName);

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

// "Receive if all sell" (CR-05). The server takes the sale fee of each sale, rounded down, so several
// partial sales can only leave the seller more than the fee on the whole total suggests. The amount is
// exact for one unit and when every unit's fee is whole (unit price × basis points divisible by
// 10,000); otherwise it is a lower bound.
struct Proceeds
{
    std::uint64_t amount = 0;
    bool exact = true;
};

Proceeds ProceedsIfAllSell(const Market::Rules& rules, std::uint32_t quantity, Market::Zen unitPrice);
// "3,186,000" or "at least 359".
std::string ProceedsText(const Proceeds& proceeds);

// The listing review: the unit price line for goods, the posting fee with its terms, the sale fee,
// "Receive if all sell" and the duration.
ListingReviewText DescribeListingReview(const Market::Rules& rules, bool goods, std::uint32_t quantity,
                                        Market::Zen unitPrice, Market::Zen postingFee, std::uint8_t durationHours);

struct FieldHint
{
    std::string text;
    bool error = false;
};

// "8 available", or "Whole number 1–8" while the entered quantity is outside that range.
FieldHint QuantityHint(std::string_view text, std::uint32_t available);
// "56 available" or "40+ available".
std::string AvailabilityText(const GoodsAvailability& availability);
// The buyer's quantity: an exact count is the ceiling (QuantityHint); a lower bound only needs a
// whole number from 1, and the quote tells how much of a larger order can be filled.
FieldHint BuyQuantityHint(std::string_view text, const GoodsAvailability& availability);
// "Need 177,000 Zen, you have 50,000".
std::string ZenShortfallText(std::uint64_t need, std::uint64_t have);
// The balance line of an open review: "Balance 33,544,050 Zen" after paying `debit` from `wallet`,
// or "Need …, you have …" once the wallet no longer covers it. Recomputed whenever the Zen changes.
std::string ReviewBalanceText(std::uint64_t debit, std::uint64_t wallet);
// "Listings 19/20".
std::string ListingCountText(std::uint8_t active, std::uint8_t maximum);
// A My auctions row: "6 remaining of 8 · 590k Zen each · 48h", or "12.5kk Zen · 48h" for an item.
std::string OwnListingLine(const Market::OwnListing& listing, std::uint32_t hoursLeft);

struct CancelReviewText
{
    std::string total, fee;
};

// The cancel review: the remaining quantity at the exact unit price, and whether the posting fee
// comes back ("Posting fee not refunded").
CancelReviewText DescribeCancelReview(const Market::OwnListing& listing, bool refundPostingFee);

// Whether Review purchase can open (D21). An invalid quantity leaves the button pressable so that a
// press focuses the quantity field; a Zen shortfall or the player's own listing disables it, with the
// reason beside it. Cannot Equip never blocks a purchase.
struct BuyReadiness
{
    bool ready = false, blocked = false, focusQuantity = false;
    std::string reason;
};

struct BuyFacts
{
    bool quantityValid = false, ownListing = false;
    // The quoted total (goods) or the listing price; 0 while no total is known.
    std::uint64_t total = 0, wallet = 0;
};

BuyReadiness DescribeBuyReadiness(const BuyFacts& facts);
// The sell check's Reason (1 not tradable, 2 no item in the slot, 3 not accepted by the market).
std::string SellCheckReasonText(std::uint8_t reason);
} // namespace UI::AuctionHouse
