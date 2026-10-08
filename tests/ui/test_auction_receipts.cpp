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
    CHECK(receipt.text == "Collected 3 deliveries · 2 items, 11,250,000 Zen");
    CHECK(receipt.tone == StatusTone::Success);

    claim.result = Market::Result::Partial;
    claim.remainingEntries = 2;
    claim.keptBack = 1;
    receipt = BuildReceipt(claim, {});
    CHECK(receipt.text == "Collected 3 deliveries · 2 items, 11,250,000 Zen · 2 deliveries left: no bag space");
    CHECK(receipt.tone == StatusTone::Error);
    claim.keptBack = 2;
    CHECK(BuildReceipt(claim, {}).text ==
          "Collected 3 deliveries · 2 items, 11,250,000 Zen · 2 deliveries left: Zen limit");
    claim.keptBack = 3;
    claim.completedEntries = 0;
    claim.itemsMoved = 0;
    claim.zenCollected = 0;
    CHECK(BuildReceipt(claim, {}).text == "Nothing collected · 2 deliveries left: no bag space, Zen limit");
}

TEST_CASE("Auction Collection counts are deliveries, not items [auction][receipts]")
{
    auto claim = MakeOutcome(Market::Operation::Claim);
    claim.completedEntries = 5;
    claim.zenCollected = 4780800;
    CHECK(BuildReceipt(claim, {}).text == "Collected 5 deliveries · 4,780,800 Zen");
    claim.completedEntries = 1;
    claim.zenCollected = 0;
    claim.itemsMoved = 2;
    CHECK(BuildReceipt(claim, {}).text == "Collected 1 delivery · 2 items");
    claim = MakeOutcome(Market::Operation::Claim, Market::Result::Partial);
    claim.remainingEntries = 1;
    claim.keptBack = 1;
    CHECK(BuildReceipt(claim, {}).text == "Nothing collected · 1 delivery left: no bag space");
}

TEST_CASE("Auction result line stays until it is replaced or cleared, never by time [auction][receipts]")
{
    StatusLine status;
    status.Show("Listed 6 × Jewel of Bless at 590,000 · fee 177,000", StatusTone::Success, StatusLink::OwnListings);
    // No lifetime: the receipt and its link stay however long the player reads (D18).
    CHECK(status.Text() == "Listed 6 × Jewel of Bless at 590,000 · fee 177,000");
    CHECK(status.Link() == StatusLink::OwnListings);
    // An error replaces the receipt and stays as well.
    status.Show("Not enough Zen.", StatusTone::Error, StatusLink::None);
    CHECK(status.Text() == "Not enough Zen.");
    CHECK(status.Tone() == StatusTone::Error);
    CHECK(status.Link() == StatusLink::None);
    // The next deliberate action, or ×, clears it.
    status.Clear();
    CHECK(status.Text().empty());
    CHECK(status.Link() == StatusLink::None);
    // An empty line never keeps a link.
    status.Show({}, StatusTone::Success, StatusLink::Collection);
    CHECK(status.Link() == StatusLink::None);
    CHECK(StatusLinkName(StatusLink::Collection) == "collection");
    CHECK(StatusLinkName(StatusLink::OwnListings) == "own");
}

TEST_CASE("Auction outcome unknown names the bag, the Collection and My auctions [auction][receipts]")
{
    CHECK(OutcomeUnknownText ==
          "Outcome unknown. Reopen Zyro and check your bag, Collection and My auctions before retrying.");
}

TEST_CASE("Auction sale and expiry notices name the item and where it went [auction][receipts]")
{
    Market::Notice notice;
    notice.kind = Market::NoticeKind::SoldToInventory;
    notice.quantity = 6;
    notice.net = 3186000;
    auto line = DescribeNotice(notice, "Jewel of Bless");
    CHECK(line.text == "Sold 6 × Jewel of Bless · 3,186,000 Zen added to your bag");
    CHECK(line.link == StatusLink::None);
    notice.kind = Market::NoticeKind::SoldToCollection;
    line = DescribeNotice(notice, "Jewel of Bless");
    CHECK(line.text == "Sold 6 × Jewel of Bless · 3,186,000 Zen waiting in your Collection");
    CHECK(line.link == StatusLink::Collection);
    notice.kind = Market::NoticeKind::ExpiredToCollection;
    notice.quantity = 1;
    notice.net = 0;
    line = DescribeNotice(notice, "Dragon Armor +9");
    CHECK(line.text == "Expired: Dragon Armor +9 · returned to your Collection");
    CHECK(line.link == StatusLink::Collection);
    notice.kind = Market::NoticeKind::ExpiredToInventory;
    notice.quantity = 4;
    CHECK(DescribeNotice(notice, "Jewel of Soul").text == "Expired: 4 × Jewel of Soul · returned to your bag");
    notice = {};
    notice.kind = Market::NoticeKind::DeliveriesWaiting;
    notice.pendingCollection = 2;
    line = DescribeNotice(notice, {});
    CHECK(line.text == "2 deliveries waiting in your Collection");
    CHECK(line.link == StatusLink::Collection);
}

TEST_CASE("Auction notices stack newest first, at most three, and never touch the result line [auction][receipts]")
{
    StatusLine status;
    status.Show("Bought 8 Jewel of Bless for 4,730,000 Zen · 8 in your bag", StatusTone::Success, StatusLink::None);
    NoticeLines notices;
    for (int i = 1; i <= 4; ++i)
        notices.Push({"Notice " + std::to_string(i), StatusLink::Collection});
    REQUIRE(notices.Lines().size() == NoticeLines::MaximumLines);
    CHECK(notices.Lines()[0].text == "Notice 4");
    CHECK(notices.Lines()[2].text == "Notice 2");
    CHECK(status.Text() == "Bought 8 Jewel of Bless for 4,730,000 Zen · 8 in your bag");
    notices.Dismiss(0);
    REQUIRE(notices.Lines().size() == 2);
    CHECK(notices.Lines()[0].text == "Notice 3");
    notices.Dismiss(5);
    CHECK(notices.Lines().size() == 2);
    notices.Clear();
    CHECK(notices.Lines().empty());
}

TEST_CASE("Auction My auctions rows and the cancel review name remaining quantity and unit price [auction][receipts]")
{
    Market::OwnListing jewels;
    jewels.kind = 2;
    jewels.listed = 8;
    jewels.remaining = 6;
    jewels.unitPrice = 590000;
    Market::OwnListing armour;
    armour.kind = 1;
    armour.listed = armour.remaining = 1;
    armour.unitPrice = 12500000;
    CHECK(OwnListingLine(jewels, 48) == "6 remaining of 8 · 590k Zen each · 48h");
    CHECK(OwnListingLine(armour, 3) == "12.5kk Zen · 3h");
    auto cancel = DescribeCancelReview(jewels, false);
    CHECK(cancel.total == "6 remaining × 590,000 Zen each");
    CHECK(cancel.fee == "Posting fee not refunded");
    cancel = DescribeCancelReview(armour, true);
    CHECK(cancel.total == "12,500,000 Zen");
    CHECK(cancel.fee == "Posting fee refunded");
}

TEST_CASE("Auction Review purchase readiness for quantity, Zen and own listings [auction][receipts]")
{
    BuyFacts facts{.quantityValid = true, .ownListing = false, .total = 4740000, .wallet = 5000000};
    auto readiness = DescribeBuyReadiness(facts);
    CHECK(readiness.ready);
    CHECK_FALSE(readiness.blocked);
    CHECK(readiness.reason.empty());
    // A shortfall disables the button with the exact amounts beside it.
    facts.wallet = 1000000;
    readiness = DescribeBuyReadiness(facts);
    CHECK_FALSE(readiness.ready);
    CHECK(readiness.blocked);
    CHECK(readiness.reason == "Need 4,740,000 Zen, you have 1,000,000");
    // An invalid quantity is not a disabled button: pressing it focuses the field.
    facts.quantityValid = false;
    readiness = DescribeBuyReadiness(facts);
    CHECK_FALSE(readiness.ready);
    CHECK_FALSE(readiness.blocked);
    CHECK(readiness.focusQuantity);
    // The player's own listing.
    facts = {.quantityValid = true, .ownListing = true, .total = 100, .wallet = 1000};
    readiness = DescribeBuyReadiness(facts);
    CHECK(readiness.blocked);
    CHECK(readiness.reason == "This is your listing.");
    // No known total yet (the quote is on its way): nothing blocks.
    facts = {.quantityValid = true, .ownListing = false, .total = 0, .wallet = 0};
    CHECK(DescribeBuyReadiness(facts).ready);
}

TEST_CASE("Auction review balance follows the wallet [auction][receipts]")
{
    // Opened at 33,545,250 Zen for a 1,200 Zen purchase.
    CHECK(ReviewBalanceText(1200, 33545250) == "Balance 33,544,050 Zen");
    // A sale notice adds 531,000 Zen while the review is open.
    CHECK(ReviewBalanceText(1200, 34076250) == "Balance 34,075,050 Zen");
    // Spending elsewhere leaves too little: the line says so instead of a negative balance.
    CHECK(ReviewBalanceText(4740000, 1000000) == "Need 4,740,000 Zen, you have 1,000,000");
    CHECK(ReviewBalanceText(5, 5) == "Balance 0 Zen");
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
                        "Receive if all sell 3,186,000\n"
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

TEST_CASE("Auction buy quantity: an exact count is a ceiling, a lower bound is not [auction][receipts]")
{
    auto hint = BuyQuantityHint("56", {56, true});
    CHECK(hint.text == "56 available");
    CHECK_FALSE(hint.error);
    CHECK(BuyQuantityHint("57", {56, true}).error);
    // Eight full tiers: "40+ available", and a larger order may still be filled.
    hint = BuyQuantityHint("57", {40, false});
    CHECK(hint.text == "40+ available");
    CHECK_FALSE(hint.error);
    CHECK(BuyQuantityHint("0", {40, false}).error);
    CHECK(BuyQuantityHint("1.5", {40, false}).error);
    CHECK(BuyQuantityHint("", {40, false}).text == "Whole number from 1");
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
