#include "AuctionFixture.h"

#include <algorithm>

using namespace GameLogic::AuctionHouse;
using AuctionFixture::Decode;
using AuctionFixture::Id;
using AuctionFixture::Load;

TEST_CASE("Auction requests match all fourteen golden fixtures [auction][network]")
{
    SearchFilter filter;
    CHECK(BuildSearchRequest(1, filter, 0) == Load("auction-search-request-empty"));
    filter.sort = 1;
    filter.slotMask = 1;
    filter.groupMask = 1;
    filter.classLine = 1;
    filter.flags = 7;
    filter.family = 2;
    filter.upgradeMin = 7;
    filter.excellentMask = 9;
    filter.minExcellentCount = 2;
    filter.maxPrice = 2147483647;
    filter.entries = {{5, 0}, {26, 0}, {4105, 1}, {4105, 2}};
    CHECK(BuildSearchRequest(0x1234, filter, 2) == Load("auction-search-request"));
    CHECK(BuildGoodsListRequest(2, {}, 0) == Load("auction-goods-list-request-all"));
    const std::array<std::uint16_t, 2> types{7181, 7182};
    CHECK(BuildGoodsListRequest(3, types, 1) == Load("auction-goods-list-request"));
    CHECK(BuildQuoteRequest(4, 7181, 0, 30) == Load("auction-goods-quote-request"));
    CHECK(BuildBuyListingRequest(6, Id(0), 1000000000) == Load("auction-buy-listing-request"));
    CHECK(BuildBuyGoodsRequest(7, 7181, 0, 18, 18000000) == Load("auction-buy-goods-request"));
    CHECK(BuildCreateListingRequest(8, 12, 18, 1000000, 24, 181000) == Load("auction-create-listing-request"));
    CHECK(BuildCancelListingRequest(9, Id(0x20)) == Load("auction-cancel-listing-request"));
    CHECK(BuildOwnListingsRequest(10) == Load("auction-own-listings-request"));
    CHECK(BuildCollectionRequest(12, 1) == Load("auction-collection-request"));
    CHECK(BuildClaimRequest(14, {}) == Load("auction-claim-request-all"));
    CHECK(BuildClaimRequest(15, Id(0x50)) == Load("auction-claim-request"));
    CHECK(BuildSellCheckRequest(16, 12) == Load("auction-sell-check-request"));
}

TEST_CASE("Auction rules announce exact limits fees and durations [auction][network]")
{
    const auto open = Decode<RulesUpdate>("auction-house-state-open");
    CHECK(open.open);
    CHECK(open.rules.pageSize == 20);
    CHECK(open.rules.maxActiveListings == 30);
    CHECK(open.rules.activeListings == 3);
    CHECK(open.rules.pendingCollection == 2);
    CHECK(open.rules.zenLimit == 2147483647);
    CHECK(open.rules.maxPrice == 2000000000);
    CHECK(open.rules.postingFeeFlat == 1000);
    CHECK(open.rules.postingFeeBasisPoints == 100);
    CHECK(open.rules.saleFeeBasisPoints == 500);
    CHECK(open.rules.refundPostingFee);
    CHECK(open.rules.durations == std::vector<std::uint8_t>{12, 24, 48, 72});
    CHECK(open.rules.priceStepPrecision == 3);
    CHECK_FALSE(Decode<RulesUpdate>("auction-house-state-closed").open);
}

TEST_CASE("Auction search steps over unequal item row lengths [auction][network]")
{
    const auto page = Decode<SearchPage>("auction-search-result");
    REQUIRE(page.rows.size() == 2);
    CHECK(page.requestId == 0x1234);
    CHECK(page.result == Result::Success);
    CHECK(page.page == 1);
    CHECK(page.totalCount == 45);
    CHECK(page.rows[0].id == Id(0));
    CHECK(page.rows[0].price == 1000000000);
    CHECK(page.rows[0].secondsLeft == 86400);
    CHECK(page.rows[0].seller == "Seller");
    CHECK(page.rows[0].requirementsMet);
    CHECK_FALSE(page.rows[0].ownListing);
    CHECK(page.rows[0].itemData == PacketBytes{0, 5, 9, 42, 15, 3, 9});
    CHECK(page.rows[1].id == Id(0x10));
    CHECK(page.rows[1].price == 2147483647);
    CHECK(page.rows[1].secondsLeft == 3600);
    CHECK(page.rows[1].seller == "Merchant10");
    CHECK(page.rows[1].requirementsMet);
    CHECK(page.rows[1].ownListing);
    CHECK(page.rows[1].itemData == PacketBytes{0, 26, 11, 64, 133, 1, 3, 1, 254, 254});
    CHECK(Decode<SearchPage>("auction-search-result-empty").rows.empty());
}

TEST_CASE("Auction goods and eight price tiers decode exact totals [auction][network]")
{
    const auto goods = Decode<GoodsPage>("auction-goods-list");
    REQUIRE(goods.rows.size() == 2);
    CHECK(goods.requestId == 3);
    CHECK(goods.page == 1);
    CHECK(goods.totalCount == 27);
    CHECK(goods.rows[0].type == 7181);
    CHECK(goods.rows[0].level == 0);
    CHECK(goods.rows[0].lowestUnitPrice == 1000000);
    CHECK(goods.rows[0].available == 18);
    CHECK(goods.rows[0].listingCount == 3);
    CHECK(goods.rows[1].type == 7182);
    CHECK(goods.rows[1].lowestUnitPrice == 2147483647);
    CHECK(goods.rows[1].available == 5);
    CHECK(goods.rows[1].listingCount == 1);
    CHECK(Decode<GoodsPage>("auction-goods-list-empty").rows.empty());
    const auto quote = Decode<Quote>("auction-goods-quote");
    CHECK(quote.requestId == 4);
    CHECK(quote.type == 7181);
    CHECK(quote.level == 0);
    CHECK(quote.requestedQuantity == 30);
    CHECK(quote.fillableQuantity == 30);
    CHECK(quote.total == 34700000);
    REQUIRE(quote.tiers.size() == 8);
    const std::array<std::uint32_t, 8> quantities{5, 4, 4, 4, 4, 3, 3, 3};
    for (std::size_t i = 0; i < quantities.size(); ++i)
    {
        CHECK(quote.tiers[i].unitPrice == 1000000 + 50000 * i);
        CHECK(quote.tiers[i].quantity == quantities[i]);
    }
    const auto empty = Decode<Quote>("auction-goods-quote-empty");
    CHECK(empty.result == Result::InsufficientStock);
    CHECK(empty.type == 7182);
    CHECK(empty.requestedQuantity == 5);
    CHECK(empty.fillableQuantity == 0);
    CHECK(empty.total == 0);
    CHECK(empty.tiers.empty());
}

TEST_CASE("Auction own listings and collection retain goods and native blobs [auction][network]")
{
    const auto own = Decode<OwnListingsPage>("auction-own-listings");
    REQUIRE(own.rows.size() == 2);
    CHECK(own.requestId == 11);
    CHECK(own.rows[0].id == Id(0x30));
    CHECK(own.rows[0].unitPrice == 2147483647);
    CHECK(own.rows[0].listed == 1);
    CHECK(own.rows[0].remaining == 1);
    CHECK(own.rows[0].secondsLeft == 172800);
    CHECK(own.rows[0].kind == 1);
    CHECK(own.rows[0].type == 5);
    CHECK(own.rows[0].level == 9);
    CHECK(own.rows[0].itemData.size() == 7);
    CHECK(own.rows[1].id == Id(0x20));
    CHECK(own.rows[1].unitPrice == 1000000);
    CHECK(own.rows[1].listed == 18);
    CHECK(own.rows[1].remaining == 10);
    CHECK(own.rows[1].secondsLeft == 43200);
    CHECK(own.rows[1].kind == 2);
    CHECK(own.rows[1].type == 7181);
    CHECK(own.rows[1].itemData.empty());
    CHECK(Decode<OwnListingsPage>("auction-own-listings-empty").rows.empty());
    const auto collection = Decode<CollectionPage>("auction-collection");
    REQUIRE(collection.rows.size() == 3);
    CHECK(collection.requestId == 13);
    CHECK(collection.totalCount == 3);
    CHECK(collection.rows[0].id == Id(0x40));
    CHECK(collection.rows[0].kind == 1);
    CHECK(collection.rows[0].type == 4105);
    CHECK(collection.rows[0].level == 7);
    CHECK(collection.rows[0].quantity == 1);
    CHECK(collection.rows[0].gross == 1000000000);
    CHECK(collection.rows[0].itemData == PacketBytes{128, 9, 7, 45, 19, 2, 81});
    CHECK(collection.rows[1].id == Id(0x50));
    CHECK(collection.rows[1].kind == 2);
    CHECK(collection.rows[1].quantity == 8);
    CHECK(collection.rows[1].gross == 8000000);
    CHECK(collection.rows[1].fee == 400000);
    CHECK(collection.rows[1].net == 7600000);
    CHECK(collection.rows[2].id == Id(0x60));
    CHECK(collection.rows[2].kind == 4);
    CHECK(collection.rows[2].type == 7182);
    CHECK(collection.rows[2].quantity == 5);
    CHECK(Decode<CollectionPage>("auction-collection-empty").rows.empty());
}

TEST_CASE("Auction mutation replies preserve delivery and partial claim amounts [auction][network]")
{
    const auto buy = Decode<Outcome>("auction-buy-listing-result");
    CHECK(buy.operation == Operation::BuyListing);
    CHECK(buy.requestId == 6);
    CHECK(buy.id == Id(0));
    CHECK(buy.paid == 1000000000);
    CHECK(buy.delivery == 1);
    const auto goods = Decode<Outcome>("auction-buy-goods-result");
    CHECK(goods.requestId == 7);
    CHECK(goods.type == 7181);
    CHECK(goods.quantity == 18);
    CHECK(goods.paid == 18000000);
    CHECK(goods.delivered == 10);
    CHECK(goods.kept == 8);
    const auto created = Decode<Outcome>("auction-create-listing-result");
    CHECK(created.id == Id(0x20));
    CHECK(created.quantity == 18);
    CHECK(created.postingFee == 181000);
    const auto cancelled = Decode<Outcome>("auction-cancel-listing-result");
    CHECK(cancelled.id == Id(0x20));
    CHECK(cancelled.delivered == 10);
    CHECK(cancelled.kept == 8);
    CHECK(cancelled.refundedFee == 181000);
    const auto claim = Decode<Outcome>("auction-claim-result");
    CHECK(claim.result == Result::Partial);
    CHECK(claim.completedEntries == 2);
    CHECK(claim.itemsMoved == 9);
    CHECK(claim.zenCollected == 1000000000);
    CHECK(claim.remainingEntries == 1);
    CHECK(claim.keptBack == 1);
}

TEST_CASE("Auction unsolicited notices and inventory sell check decode [auction][network]")
{
    const auto sold = Decode<Notice>("auction-notice-sold");
    CHECK(sold.kind == NoticeKind::SoldToCollection);
    CHECK(sold.type == 7181);
    CHECK(sold.level == 0);
    CHECK(sold.quantity == 8);
    CHECK(sold.net == 7600000);
    CHECK(sold.pendingCollection == 2);
    const auto waiting = Decode<Notice>("auction-notice-deliveries-waiting");
    CHECK(waiting.kind == NoticeKind::DeliveriesWaiting);
    CHECK(waiting.pendingCollection == 3);
    const auto check = Decode<SellCheck>("auction-sell-check-result");
    CHECK(check.requestId == 16);
    CHECK(check.inventorySlot == 12);
    CHECK(check.reason == 0);
    CHECK(check.kind == 2);
    CHECK(check.type == 7181);
    CHECK(check.level == 0);
    CHECK(check.available == 18);
    CHECK(check.lowestUnitPrice == 1000000);
    CHECK(check.activeListings == 3);
}

TEST_CASE("Auction amended result and inventory notices decode without changing layout [auction][network]")
{
    auto sold = Load("auction-notice-sold");
    sold[4] = static_cast<std::uint8_t>(NoticeKind::SoldToInventory);
    const auto decodedSold = ParseServerPacket(sold);
    REQUIRE(decodedSold.has_value());
    CHECK(std::get<Notice>(*decodedSold).kind == NoticeKind::SoldToInventory);
    CHECK(std::get<Notice>(*decodedSold).net == 7600000);
    sold[4] = static_cast<std::uint8_t>(NoticeKind::ExpiredToInventory);
    std::fill(sold.begin() + 12, sold.begin() + 16, 0);
    const auto expired = ParseServerPacket(sold);
    REQUIRE(expired.has_value());
    CHECK(std::get<Notice>(*expired).kind == NoticeKind::ExpiredToInventory);
    CHECK(std::get<Notice>(*expired).quantity == 8);
    CHECK(std::get<Notice>(*expired).net == 0);
    sold[4] = 6;
    CHECK_FALSE(ParseServerPacket(sold).has_value());

    auto refused = Load("auction-create-listing-result");
    refused[6] = static_cast<std::uint8_t>(Result::PriceNotOnStep);
    std::fill(refused.begin() + 7, refused.end(), 0);
    const auto result = ParseServerPacket(refused);
    REQUIRE(result.has_value());
    CHECK(std::get<Outcome>(*result).result == Result::PriceNotOnStep);
    CHECK(std::get<Outcome>(*result).postingFee == 0);
    refused[6] = 17;
    CHECK_FALSE(ParseServerPacket(refused).has_value());
}

TEST_CASE("Auction parser refuses every truncated fixture and trailing bytes [auction][network]")
{
    for (const auto& entry : std::filesystem::directory_iterator(AUCTION_FIXTURE_DIR))
    {
        const auto name = entry.path().stem().string();
        if (name.find("request") != std::string::npos) continue;
        auto packet = Load(name);
        REQUIRE(ParseServerPacket(packet).has_value());
        for (std::size_t size = 0; size < packet.size(); ++size)
        {
            CAPTURE(name);
            CAPTURE(size);
            CHECK_FALSE(ParseServerPacket(std::span(packet).first(size)).has_value());
        }
        packet.push_back(0);
        CHECK_FALSE(ParseServerPacket(packet).has_value());
        if (packet[0] == 0xC1) packet[1] = static_cast<std::uint8_t>(packet.size());
        else packet[2] = static_cast<std::uint8_t>(packet.size());
        CHECK_FALSE(ParseServerPacket(packet).has_value());
    }
}

TEST_CASE("Auction rejects corrupt counts framing and unsafe native item lengths [auction][network]")
{
    auto search = Load("auction-search-result");
    search[50] = 6; // Blob prefix would truncate the excellent byte.
    CHECK_FALSE(ParseServerPacket(search).has_value());
    auto quote = Load("auction-goods-quote");
    quote[22] = 9;
    CHECK_FALSE(ParseServerPacket(quote).has_value());
    auto unknown = Load("auction-house-state-open");
    unknown[3] = 0x5F;
    CHECK_FALSE(ParseServerPacket(unknown).has_value());
    unknown[3] = 0x50;
    unknown[2] = 0x7A;
    CHECK_FALSE(ParseServerPacket(unknown).has_value());
    CHECK_FALSE(IsValidItemData(PacketBytes{0, 5, 9, 42, 0x80}));
    CHECK_FALSE(IsValidItemData(PacketBytes{0, 5, 9, 42, 0x80, 6, 1, 1, 1, 1, 1, 1}));
    SearchFilter filter;
    filter.entries.resize(49);
    CHECK(BuildSearchRequest(1, filter, 0).empty());
    filter.entries.clear();
    CHECK(BuildSearchRequest(0, filter, 0).empty());
    filter.upgradeMin = 16;
    CHECK(BuildSearchRequest(1, filter, 0).empty());
}

TEST_CASE("Auction search supports all four agreed sorts and rejects unknown values [auction][network]")
{
    SearchFilter filter;
    const auto original = Load("auction-search-request-empty");
    for (std::uint8_t sort = 0; sort < 4; ++sort)
    {
        auto expected = original;
        expected[8] = sort;
        filter.sort = sort;
        CHECK(BuildSearchRequest(1, filter, 0) == expected);
    }
    filter.sort = 4;
    CHECK(BuildSearchRequest(1, filter, 0).empty());
}
