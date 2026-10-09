#include "UI/AuctionHouse/Receipts.h"

#include "GameLogic/AuctionHouse/AuctionHouse.h"
#include "UI/AuctionHouse/Presentation.h"

#include <algorithm>
#include <limits>

namespace UI::AuctionHouse
{
namespace
{
constexpr std::uint8_t KeptBackInventorySpace = 1, KeptBackZenLimit = 2;
constexpr std::uint8_t DeliveryInventory = 1, DeliveryCollection = 2;
constexpr std::uint8_t GoodsListingKind = 2;

std::string Count(std::uint32_t count, std::string_view one, std::string_view many)
{
    return std::to_string(count) + " " + std::string(count == 1 ? one : many);
}

Receipt Bought(const Market::Outcome& outcome, const ReceiptSubject& subject)
{
    Receipt receipt;
    receipt.text =
        "Bought " + std::to_string(outcome.quantity) + " " + subject.name + " for " + ExactZen(outcome.paid) + " Zen";
    if (outcome.kept == 0)
        receipt.text += " · " + std::to_string(outcome.delivered) + " in your bag";
    else if (outcome.delivered == 0)
        receipt.text += " · " + std::to_string(outcome.kept) + " in Collection";
    else
        receipt.text += " · " + std::to_string(outcome.delivered) + " in your bag, " + std::to_string(outcome.kept) +
                        " in Collection";
    if (outcome.kept)
    {
        receipt.link = StatusLink::Collection;
        receipt.collectionEntries = 1;
    }
    return receipt;
}

Receipt BoughtItem(const Market::Outcome& outcome, const ReceiptSubject& subject)
{
    Receipt receipt;
    receipt.text = "Bought " + subject.name + " for " + ExactZen(outcome.paid) + " Zen";
    if (outcome.delivery == DeliveryInventory)
        receipt.text += " · in your bag";
    else if (outcome.delivery == DeliveryCollection)
    {
        receipt.text += " · in Collection (bag full)";
        receipt.link = StatusLink::Collection;
        receipt.collectionEntries = 1;
    }
    return receipt;
}

Receipt Listed(const Market::Outcome& outcome, const ReceiptSubject& subject)
{
    Receipt receipt;
    receipt.text = "Listed " + (outcome.quantity > 1 ? std::to_string(outcome.quantity) + " × " : std::string{}) +
                   subject.name + " at " + ExactZen(subject.unitPrice) + " · fee " + ExactZen(outcome.postingFee);
    receipt.link = StatusLink::OwnListings;
    return receipt;
}

Receipt Cancelled(const Market::Outcome& outcome, const ReceiptSubject& subject)
{
    Receipt receipt;
    receipt.text = "Cancelled " + subject.name;
    const auto bag = outcome.delivered, collection = outcome.kept;
    if (bag + collection == 1)
        receipt.text += bag ? " · returned to your bag" : " · returned to Collection";
    else if (!collection)
        receipt.text += " · " + std::to_string(bag) + " returned to your bag";
    else if (!bag)
        receipt.text += " · " + std::to_string(collection) + " returned to Collection";
    else
        receipt.text +=
            " · " + std::to_string(bag) + " returned to your bag, " + std::to_string(collection) + " to Collection";
    if (outcome.refundedFee)
        receipt.text += " · fee " + ExactZen(outcome.refundedFee) + " refunded";
    if (collection)
    {
        receipt.link = StatusLink::Collection;
        receipt.collectionEntries = 1;
    }
    return receipt;
}

Receipt Collected(const Market::Outcome& outcome)
{
    // Collection counts are deliveries (entries), not units: one delivery can hold several items (D22).
    Receipt receipt;
    std::string moved;
    if (outcome.itemsMoved)
        moved = Count(outcome.itemsMoved, "item", "items");
    if (outcome.zenCollected)
        moved += (moved.empty() ? "" : ", ") + ExactZen(outcome.zenCollected) + " Zen";
    receipt.text = outcome.completedEntries || !moved.empty()
                       ? "Collected " + Count(outcome.completedEntries, "delivery", "deliveries") +
                             (moved.empty() ? "" : " · " + moved)
                       : "Nothing collected";
    if (outcome.result != Market::Result::Partial)
        return receipt;
    receipt.tone = StatusTone::Error;
    receipt.text += " · " + Count(outcome.remainingEntries, "delivery", "deliveries") + " left: ";
    const bool space = outcome.keptBack & KeptBackInventorySpace, zen = outcome.keptBack & KeptBackZenLimit;
    receipt.text += space && zen ? "no bag space, Zen limit" : space ? "no bag space" : zen ? "Zen limit" : "kept back";
    return receipt;
}

std::string NoticeSubject(const Market::Notice& notice, std::string_view itemName)
{
    const std::string name = itemName.empty() ? "an item" : std::string(itemName);
    return notice.quantity > 1 ? std::to_string(notice.quantity) + " × " + name : name;
}
} // namespace

std::string_view StatusLinkName(StatusLink link)
{
    switch (link)
    {
    case StatusLink::Collection:
        return "collection";
    case StatusLink::OwnListings:
        return "own";
    default:
        return {};
    }
}

void StatusLine::Show(std::string text, StatusTone tone, StatusLink link)
{
    m_Text = std::move(text);
    m_Tone = tone;
    m_Link = m_Text.empty() ? StatusLink::None : link;
}

void StatusLine::Clear()
{
    *this = {};
}

void NoticeLines::Push(NoticeLine line)
{
    line.announcement = false;
    m_Sales.insert(m_Sales.begin(), std::move(line));
    if (m_Sales.size() > MaximumLines)
        m_Sales.resize(MaximumLines);
    Compose();
}

void NoticeLines::PushAnnouncement(std::string text)
{
    m_Announcement = NoticeLine{std::move(text), StatusLink::None, true};
    Compose();
}

void NoticeLines::Dismiss(std::size_t index)
{
    if (m_Announcement && index == 0)
        m_Announcement.reset();
    else if (const auto sale = index - (m_Announcement ? 1 : 0); sale < m_Sales.size())
        m_Sales.erase(m_Sales.begin() + static_cast<std::ptrdiff_t>(sale));
    Compose();
}

void NoticeLines::Clear()
{
    m_Announcement.reset();
    m_Sales.clear();
    m_Lines.clear();
}

void NoticeLines::Compose()
{
    m_Lines.clear();
    if (m_Announcement)
        m_Lines.push_back(*m_Announcement);
    m_Lines.insert(m_Lines.end(), m_Sales.begin(), m_Sales.end());
}

NoticeLine DescribeNotice(const Market::Notice& notice, std::string_view itemName)
{
    using K = Market::NoticeKind;
    switch (notice.kind)
    {
    case K::SoldToInventory:
        return {"Sold " + NoticeSubject(notice, itemName) + " · " + ExactZen(notice.net) + " Zen added to your bag"};
    case K::SoldToCollection:
        return {"Sold " + NoticeSubject(notice, itemName) + " · " + ExactZen(notice.net) +
                    " Zen waiting in your Collection",
                StatusLink::Collection};
    case K::ExpiredToInventory:
        return {"Expired: " + NoticeSubject(notice, itemName) + " · returned to your bag"};
    case K::ExpiredToCollection:
        return {"Expired: " + NoticeSubject(notice, itemName) + " · returned to your Collection",
                StatusLink::Collection};
    case K::DeliveriesWaiting:
        return {Count(notice.pendingCollection, "delivery", "deliveries") + " waiting in your Collection",
                StatusLink::Collection};
    default:
        return {};
    }
}

Receipt BuildReceipt(const Market::Outcome& outcome, const ReceiptSubject& subject)
{
    using O = Market::Operation;
    switch (outcome.operation)
    {
    case O::BuyGoods:
        return Bought(outcome, subject);
    case O::BuyListing:
        return BoughtItem(outcome, subject);
    case O::CreateListing:
        return Listed(outcome, subject);
    case O::CancelListing:
        return Cancelled(outcome, subject);
    case O::Claim:
        return Collected(outcome);
    default:
        return {};
    }
}

std::string PercentText(std::uint16_t basisPoints)
{
    auto text = std::to_string(basisPoints / 100);
    if (const auto fraction = basisPoints % 100)
    {
        text += fraction < 10 ? ".0" + std::to_string(fraction) : "." + std::to_string(fraction);
        if (text.back() == '0')
            text.pop_back();
    }
    return text + " %";
}

Proceeds ProceedsIfAllSell(const Market::Rules& rules, std::uint32_t quantity, Market::Zen unitPrice)
{
    constexpr std::uint64_t BasisPointDivisor = 10000;
    const auto total = std::uint64_t{quantity} * unitPrice;
    // Above the Zen limit no listing is possible; the review then shows no fee, as before.
    const std::uint64_t fee =
        total <= rules.zenLimit
            ? std::min<std::uint64_t>(total, Market::SaleFee(rules, static_cast<Market::Zen>(total)))
            : 0;
    return {total - fee, quantity <= 1 || std::uint64_t{unitPrice} * rules.saleFeeBasisPoints % BasisPointDivisor == 0};
}

std::string ProceedsText(const Proceeds& proceeds)
{
    return (proceeds.exact ? "" : "at least ") + ExactZen(proceeds.amount);
}

ListingReviewText DescribeListingReview(const Market::Rules& rules, bool goods, std::uint32_t quantity,
                                        Market::Zen unitPrice, Market::Zen postingFee, std::uint8_t durationHours)
{
    const auto total = static_cast<std::uint64_t>(quantity) * unitPrice;
    ListingReviewText text;
    text.total = goods ? std::to_string(quantity) + " × " + ExactZen(unitPrice) + " = " + ExactZen(total) + " Zen"
                       : ExactZen(total) + " Zen";
    std::string terms;
    if (rules.postingFeeBasisPoints)
        terms = PercentText(rules.postingFeeBasisPoints);
    if (rules.postingFeeFlat)
        terms += (terms.empty() ? "" : " + ") + ExactZen(rules.postingFeeFlat);
    terms += (terms.empty() ? "" : ", ") + std::string("charged now, ") +
             (rules.refundPostingFee ? "refunded on cancel" : "not refunded");
    const auto saleFee = total <= rules.zenLimit ? Market::SaleFee(rules, static_cast<Market::Zen>(total)) : 0;
    text.fees = "Posting fee " + ExactZen(postingFee) + " (" + terms + ")\nSale fee " + ExactZen(saleFee) + " (" +
                PercentText(rules.saleFeeBasisPoints) + ", when sold)\nReceive if all sell " +
                ProceedsText(ProceedsIfAllSell(rules, quantity, unitPrice)) + "\nDuration " +
                std::to_string(durationHours) + " h";
    return text;
}

FieldHint QuantityHint(std::string_view text, std::uint32_t available)
{
    if (ParseQuantity(text, available))
        return {std::to_string(available) + " available", false};
    if (available == 0)
        return {"None available", true};
    return {available == 1 ? "Whole number 1" : "Whole number 1–" + std::to_string(available), true};
}

std::string AvailabilityText(const GoodsAvailability& availability)
{
    return std::to_string(availability.count) + (availability.exact ? " available" : "+ available");
}

FieldHint BuyQuantityHint(std::string_view text, const GoodsAvailability& availability)
{
    if (availability.exact)
        return QuantityHint(text, availability.count);
    if (ParseQuantity(text, std::numeric_limits<std::uint32_t>::max()))
        return {AvailabilityText(availability), false};
    return {"Whole number from 1", true};
}

std::string ZenShortfallText(std::uint64_t need, std::uint64_t have)
{
    return "Need " + ExactZen(need) + " Zen, you have " + ExactZen(have);
}

std::string ReviewBalanceText(std::uint64_t debit, std::uint64_t wallet)
{
    if (debit > wallet)
        return ZenShortfallText(debit, wallet);
    return "Balance " + ExactZen(wallet - debit) + " Zen";
}

std::string ListingCountText(std::uint8_t active, std::uint8_t maximum)
{
    return "Listings " + std::to_string(active) + "/" + std::to_string(maximum);
}

std::string OwnListingLine(const Market::OwnListing& listing, std::uint32_t hoursLeft)
{
    const auto hours = std::to_string(hoursLeft) + "h";
    if (listing.kind != GoodsListingKind)
        return CompactZen(listing.unitPrice) + " Zen · " + hours;
    return std::to_string(listing.remaining) + " remaining of " + std::to_string(listing.listed) + " · " +
           CompactZen(listing.unitPrice) + " Zen each · " + hours;
}

CancelReviewText DescribeCancelReview(const Market::OwnListing& listing, bool refundPostingFee)
{
    CancelReviewText text;
    text.total = listing.kind == GoodsListingKind
                     ? std::to_string(listing.remaining) + " remaining × " + ExactZen(listing.unitPrice) + " Zen each"
                     : ExactZen(listing.unitPrice) + " Zen";
    text.fee = refundPostingFee ? "Posting fee refunded" : "Posting fee not refunded";
    return text;
}

BuyReadiness DescribeBuyReadiness(const BuyFacts& facts)
{
    BuyReadiness readiness;
    if (facts.ownListing)
    {
        readiness.blocked = true;
        readiness.reason = "This is your listing.";
        return readiness;
    }
    if (!facts.quantityValid)
    {
        readiness.focusQuantity = true;
        return readiness;
    }
    if (facts.total > facts.wallet)
    {
        readiness.blocked = true;
        readiness.reason = ZenShortfallText(facts.total, facts.wallet);
        return readiness;
    }
    readiness.ready = true;
    return readiness;
}

std::string SellCheckReasonText(std::uint8_t reason)
{
    switch (reason)
    {
    case 1:
        return "This item can't be traded.";
    case 2:
        return "Item moved. Select it again.";
    case 3:
        return "The market doesn't accept this item.";
    default:
        return "This item cannot be listed.";
    }
}
} // namespace UI::AuctionHouse
