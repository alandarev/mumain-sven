#include "UI/AuctionHouse/Receipts.h"

#include "GameLogic/AuctionHouse/AuctionHouse.h"
#include "UI/AuctionHouse/Presentation.h"

namespace UI::AuctionHouse
{
namespace
{
constexpr std::uint8_t KeptBackInventorySpace = 1, KeptBackZenLimit = 2;
constexpr std::uint8_t DeliveryInventory = 1, DeliveryCollection = 2;

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
    Receipt receipt;
    std::string moved;
    if (outcome.itemsMoved)
        moved = Count(outcome.itemsMoved, "item", "items");
    if (outcome.zenCollected)
        moved += (moved.empty() ? "" : ", ") + ExactZen(outcome.zenCollected) + " Zen";
    receipt.text = outcome.completedEntries || !moved.empty()
                       ? "Collected " + std::to_string(outcome.completedEntries) + (moved.empty() ? "" : " · " + moved)
                       : "Nothing collected";
    if (outcome.result != Market::Result::Partial)
        return receipt;
    receipt.tone = StatusTone::Error;
    receipt.text += " · " + std::to_string(outcome.remainingEntries) + " left: ";
    const bool space = outcome.keptBack & KeptBackInventorySpace, zen = outcome.keptBack & KeptBackZenLimit;
    receipt.text += space && zen ? "bag full, Zen limit" : space ? "bag full" : zen ? "Zen limit" : "kept back";
    return receipt;
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

void StatusLine::Show(std::string text, StatusTone tone, StatusLink link, Time now)
{
    m_Text = std::move(text);
    m_Tone = tone;
    m_Link = link;
    m_Until = m_Text.empty() || tone == StatusTone::Error ? Time{} : now + Lifetime;
}

void StatusLine::Clear()
{
    *this = {};
}

bool StatusLine::Expire(Time now)
{
    if (m_Until == Time{} || now < m_Until)
        return false;
    Clear();
    return true;
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
                PercentText(rules.saleFeeBasisPoints) + ", when sold)\nYou receive " + ExactZen(total - saleFee) +
                " if all sell\nDuration " + std::to_string(durationHours) + " h";
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

std::string ZenShortfallText(std::uint64_t need, std::uint64_t have)
{
    return "Need " + ExactZen(need) + " Zen, you have " + ExactZen(have);
}

std::string ListingCountText(std::uint8_t active, std::uint8_t maximum)
{
    return "Listings " + std::to_string(active) + "/" + std::to_string(maximum);
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
