#include <doctest.h>
#include "UI/AuctionHouse/Receipts.h"

using namespace UI::AuctionHouse;

namespace
{
Market::Outcome MakeOutcome(Market::Operation operation, Market::Result result = Market::Result::Success)
{
    Market::Outcome outcome;
    outcome.operation = operation;
    outcome.result = result;
    return outcome;
}
} // namespace

TEST_CASE("Auction goods purchase receipt says where the units went [auction][receipts]")
{
    auto outcome = MakeOutcome(Market::Operation::BuyGoods);
    outcome.quantity = 8;
    outcome.paid = 4730000;
    outcome.delivered = 8;
    const ReceiptSubject bless{"Jewel of Bless", 0};
    auto receipt = BuildReceipt(outcome, bless);
    CHECK(receipt.text == "Bought 8 Jewel of Bless for 4,730,000 Zen · 8 in your bag");
    CHECK(receipt.link == StatusLink::None);
    CHECK(receipt.collectionEntries == 0);
    CHECK(receipt.tone == StatusTone::Success);

    outcome.delivered = 6;
    outcome.kept = 2;
    receipt = BuildReceipt(outcome, bless);
    CHECK(receipt.text == "Bought 8 Jewel of Bless for 4,730,000 Zen · 6 in your bag, 2 in Collection");
    CHECK(receipt.link == StatusLink::Collection);
    CHECK(receipt.collectionEntries == 1);

    outcome.delivered = 0;
    outcome.kept = 8;
    CHECK(BuildReceipt(outcome, bless).text == "Bought 8 Jewel of Bless for 4,730,000 Zen · 8 in Collection");
}

TEST_CASE("Auction item purchase listing and cancel receipts use the result fields [auction][receipts]")
{
    auto bought = MakeOutcome(Market::Operation::BuyListing);
    bought.paid = 12500000;
    bought.delivery = 1;
    CHECK(BuildReceipt(bought, {"Dragon Armor +9", 0}).text ==
          "Bought Dragon Armor +9 for 12,500,000 Zen · in your bag");
    bought.delivery = 2;
    const auto kept = BuildReceipt(bought, {"Dragon Armor +9", 0});
    CHECK(kept.text == "Bought Dragon Armor +9 for 12,500,000 Zen · in Collection (bag full)");
    CHECK(kept.link == StatusLink::Collection);
    CHECK(kept.collectionEntries == 1);

    auto listed = MakeOutcome(Market::Operation::CreateListing);
    listed.quantity = 6;
    listed.postingFee = 177000;
    const auto receipt = BuildReceipt(listed, {"Jewel of Bless", 590000});
    CHECK(receipt.text == "Listed 6 × Jewel of Bless at 590,000 · fee 177,000");
    CHECK(receipt.link == StatusLink::OwnListings);
    listed.quantity = 1;
    listed.postingFee = 625000;
    CHECK(BuildReceipt(listed, {"Dragon Armor +9", 12500000}).text ==
          "Listed Dragon Armor +9 at 12,500,000 · fee 625,000");

    auto cancelled = MakeOutcome(Market::Operation::CancelListing);
    cancelled.delivered = 6;
    CHECK(BuildReceipt(cancelled, {"Jewel of Bless", 0}).text == "Cancelled Jewel of Bless · 6 returned to your bag");
    cancelled.delivered = 4;
    cancelled.kept = 2;
    cancelled.refundedFee = 177000;
    const auto split = BuildReceipt(cancelled, {"Jewel of Bless", 0});
    CHECK(split.text == "Cancelled Jewel of Bless · 4 returned to your bag, 2 to Collection · fee 177,000 refunded");
    CHECK(split.link == StatusLink::Collection);
    cancelled.delivered = 0;
    cancelled.kept = 1;
    cancelled.refundedFee = 0;
    CHECK(BuildReceipt(cancelled, {"Dragon Armor +9", 0}).text == "Cancelled Dragon Armor +9 · returned to Collection");
}

TEST_CASE("Auction claim receipt names counts and the reasons anything was kept back [auction][receipts]")
{
    auto claim = MakeOutcome(Market::Operation::Claim);
    claim.completedEntries = 3;
    claim.itemsMoved = 2;
    claim.zenCollected = 11250000;
    auto receipt = BuildReceipt(claim, {});
    CHECK(receipt.text == "Collected 3 · 2 items, 11,250,000 Zen");
    CHECK(receipt.tone == StatusTone::Success);

    claim.result = Market::Result::Partial;
    claim.remainingEntries = 2;
    claim.keptBack = 1;
    receipt = BuildReceipt(claim, {});
    CHECK(receipt.text == "Collected 3 · 2 items, 11,250,000 Zen · 2 left: bag full");
    CHECK(receipt.tone == StatusTone::Error);
    claim.keptBack = 2;
    CHECK(BuildReceipt(claim, {}).text == "Collected 3 · 2 items, 11,250,000 Zen · 2 left: Zen limit");
    claim.keptBack = 3;
    claim.completedEntries = 0;
    claim.itemsMoved = 0;
    claim.zenCollected = 0;
    CHECK(BuildReceipt(claim, {}).text == "Nothing collected · 2 left: bag full, Zen limit");
}

TEST_CASE("Auction status replacement cancels the earlier expiry and errors persist [auction][receipts]")
{
    StatusLine status;
    const auto start = RequestClock::Time{} + std::chrono::seconds(100);
    status.Show("Listed 6 × Jewel of Bless at 590,000 · fee 177,000", StatusTone::Success, StatusLink::OwnListings,
                start);
    CHECK_FALSE(status.Expire(start + std::chrono::seconds(7)));
    // An error replaces the receipt; the receipt's expiry must not clear it.
    status.Show("Not enough Zen.", StatusTone::Error, StatusLink::None, start + std::chrono::seconds(7));
    CHECK_FALSE(status.Expire(start + StatusLine::Lifetime));
    CHECK_FALSE(status.Expire(start + std::chrono::minutes(10)));
    CHECK(status.Text() == "Not enough Zen.");
    CHECK(status.Link() == StatusLink::None);
    // A new success restarts the lifetime from its own time.
    status.Show("Collected 1", StatusTone::Success, StatusLink::None, start + std::chrono::minutes(10));
    CHECK_FALSE(status.Expire(start + std::chrono::minutes(10) + std::chrono::seconds(7)));
    CHECK(status.Expire(start + std::chrono::minutes(10) + StatusLine::Lifetime));
    CHECK(status.Text().empty());
    CHECK(StatusLinkName(StatusLink::Collection) == "collection");
    CHECK(StatusLinkName(StatusLink::OwnListings) == "own");
    CHECK(OutcomeUnknownText.starts_with("Outcome unknown"));
}

TEST_CASE("Auction listing review shows the unit price line fee terms and proceeds [auction][receipts]")
{
    Market::Rules rules;
    rules.zenLimit = 2000000000;
    rules.maxPrice = 2000000000;
    rules.postingFeeBasisPoints = 500;
    rules.saleFeeBasisPoints = 1000;
    const auto goods = DescribeListingReview(rules, true, 6, 590000, 177000, 48);
    CHECK(goods.total == "6 × 590,000 = 3,540,000 Zen");
    CHECK(goods.fees == "Posting fee 177,000 (5 %, charged now, not refunded)\n"
                        "Sale fee 354,000 (10 %, when sold)\n"
                        "You receive 3,186,000 if all sell\n"
                        "Duration 48 h");
    rules.postingFeeFlat = 1000;
    rules.refundPostingFee = true;
    rules.postingFeeBasisPoints = 250;
    const auto item = DescribeListingReview(rules, false, 1, 12500000, 313500, 24);
    CHECK(item.total == "12,500,000 Zen");
    CHECK(item.fees.starts_with("Posting fee 313,500 (2.5 % + 1,000, charged now, refunded on cancel)\n"));
    CHECK(PercentText(25) == "0.25 %");
    CHECK(PercentText(5) == "0.05 %");
    CHECK(PercentText(1000) == "10 %");
}

TEST_CASE("Auction inline validation names range shortfall count and sell reasons [auction][receipts]")
{
    auto hint = QuantityHint("8", 8);
    CHECK(hint.text == "8 available");
    CHECK_FALSE(hint.error);
    hint = QuantityHint("9", 8);
    CHECK(hint.text == "Whole number 1–8");
    CHECK(hint.error);
    CHECK(QuantityHint("1.5", 8).error);
    CHECK(QuantityHint("0", 8).error);
    CHECK(ZenShortfallText(177000, 50000) == "Need 177,000 Zen, you have 50,000");
    CHECK(ListingCountText(19, 20) == "Listings 19/20");
    CHECK(SellCheckReasonText(1) == "This item can't be traded.");
    CHECK(SellCheckReasonText(2) == "Item moved. Select it again.");
    CHECK(SellCheckReasonText(3) == "The market doesn't accept this item.");
}
