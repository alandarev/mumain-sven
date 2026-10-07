#include "AuctionFixture.h"
#include "GameLogic/AuctionHouse/AuctionHouse.h"
#include "GameLogic/AuctionHouse/AuctionHouseTransport.h"
#include "GameLogic/AuctionHouse/AuctionHouseNameSearch.h"

#include <algorithm>
#include <limits>

using namespace GameLogic::AuctionHouse;
using AuctionFixture::Id;
using AuctionFixture::Load;

namespace
{
std::vector<PacketBytes> sent;
bool transportReady = true;
int closeCount = 0;

struct Listener : IListener
{
    int opened = 0, closed = 0;
    std::vector<Operation> changed;
    std::vector<Notice> notices;
    void OnOpened() override { ++opened; }
    void OnClosed() override { ++closed; }
    void OnChanged(Operation op) override { changed.push_back(op); }
    void OnNotice(const Notice& notice) override { notices.push_back(notice); }
};

struct Session
{
    Listener listener;
    Session()
    {
        SetListener(nullptr);
        Reset();
        sent.clear();
        closeCount = 0;
        transportReady = true;
        SetListener(&listener);
        ReceivePacket(Load("auction-house-state-open"));
    }
    ~Session() { SetListener(nullptr); Reset(); }
};

std::uint16_t LastId()
{
    REQUIRE_FALSE(sent.empty());
    return static_cast<std::uint16_t>(sent.back()[4] | (std::uint16_t{sent.back()[5]} << 8));
}

PacketBytes ReplyWithId(std::string_view fixture, std::uint16_t id)
{
    auto packet = Load(fixture);
    const auto offset = packet[0] == 0xC2 ? 5u : 4u;
    packet[offset] = static_cast<std::uint8_t>(id);
    packet[offset + 1] = static_cast<std::uint8_t>(id >> 8);
    return packet;
}

void ReceiveLatest(std::string_view fixture) { ReceivePacket(ReplyWithId(fixture, LastId())); }
} // namespace

namespace GameLogic::AuctionHouse::Transport
{
bool Send(std::span<const std::uint8_t> bytes)
{
    if (!transportReady) return false;
    sent.emplace_back(bytes.begin(), bytes.end());
    return true;
}
void CloseDialog() { ++closeCount; }
} // namespace GameLogic::AuctionHouse::Transport

TEST_CASE("Auction closed state refuses requests and still delivers notices [auction][state]")
{
    Session session;
    ReceivePacket(Load("auction-house-state-closed"));
    CHECK_FALSE(GetState().IsOpen());
    CHECK(session.listener.closed == 1);
    CHECK_FALSE(RequestSearch({}, 0));
    CHECK_FALSE(BuyListing(Id(0), 1000000000));
    CHECK_FALSE(Claim({}));
    CHECK(sent.empty());
    ReceivePacket(Load("auction-notice-sold"));
    REQUIRE(session.listener.notices.size() == 1);
    CHECK(session.listener.notices[0].net == 7600000);
    CHECK(GetState().GetRules().pendingCollection == 2);
}

TEST_CASE("Auction latest search wins and duplicate answers do nothing [auction][state]")
{
    Session session;
    REQUIRE(RequestSearch({}, 0));
    const auto first = LastId();
    REQUIRE(RequestSearch({}, 1));
    const auto second = LastId();
    CHECK(second != first);
    const auto revision = GetState().Revision();
    ReceivePacket(ReplyWithId("auction-search-result", first));
    CHECK(GetState().Revision() == revision);
    CHECK(GetState().IsPending(Operation::Search));
    CHECK(GetState().Search().rows.empty());
    ReceivePacket(ReplyWithId("auction-search-result", second));
    CHECK_FALSE(GetState().IsPending(Operation::Search));
    REQUIRE(GetState().Search().rows.size() == 2);
    CHECK(GetState().Search().rows[0].seller == "Seller");
    CHECK(GetState().Search().receivedAt != ReceivedTime{});
    const auto acceptedRevision = GetState().Revision();
    ReceivePacket(ReplyWithId("auction-search-result", second));
    CHECK(GetState().Revision() == acceptedRevision);
}

TEST_CASE("Auction allows overlapping reads and locks all mutations [auction][state]")
{
    Session session;
    REQUIRE(BuyListing(Id(0), 1000000000));
    const auto id = LastId();
    CHECK_FALSE(BuyListing(Id(0), 1000000000));
    CHECK_FALSE(BuyGoods(7181, 0, 18, 18000000));
    CHECK_FALSE(CreateListing(12, 18, 1000000, 24, 181000));
    CHECK_FALSE(CancelListing(Id(0x20)));
    CHECK_FALSE(Claim({}));
    CHECK(sent.size() == 1);
    CHECK(RequestSearch({}, 0));
    CHECK(RequestGoodsList({}, 0));
    CHECK(RequestOwnListings());
    CHECK(RequestCollection(0));
    ReceivePacket(ReplyWithId("auction-buy-listing-result", id));
    CHECK_FALSE(GetState().IsPending(Operation::BuyListing));
    REQUIRE(GetState().LastOutcome(Operation::BuyListing).has_value());
    CHECK(GetState().LastOutcome(Operation::BuyListing)->paid == 1000000000);
    CHECK(GetState().LastOutcome(Operation::BuyListing)->delivery == 1);
    CHECK(Claim({}));
}

TEST_CASE("Auction purchase refusal accepts its fresh quote exactly once [auction][state]")
{
    Session session;
    REQUIRE(BuyGoods(7181, 0, 30, 34700000));
    const auto id = LastId();
    auto refused = ReplyWithId("auction-buy-goods-result", id);
    refused[6] = static_cast<std::uint8_t>(Result::QuoteChanged);
    ReceivePacket(refused);
    CHECK_FALSE(GetState().IsPending(Operation::BuyGoods));
    CHECK(GetState().LastOutcome(Operation::BuyGoods)->result == Result::QuoteChanged);
    CHECK_FALSE(GetState().CurrentQuote().has_value());
    ReceivePacket(ReplyWithId("auction-goods-quote", id));
    REQUIRE(GetState().CurrentQuote().has_value());
    CHECK(GetState().CurrentQuote()->total == 34700000);
    const auto revision = GetState().Revision();
    ReceivePacket(ReplyWithId("auction-goods-quote", id));
    CHECK(GetState().Revision() == revision);
    REQUIRE(RequestQuote(7182, 0, 5));
    CHECK_FALSE(GetState().CurrentQuote().has_value());
    ReceivePacket(ReplyWithId("auction-goods-quote", id));
    CHECK_FALSE(GetState().CurrentQuote().has_value());
    ReceiveLatest("auction-goods-quote-empty");
    REQUIRE(GetState().CurrentQuote().has_value());
    CHECK(GetState().CurrentQuote()->result == Result::InsufficientStock);
}

TEST_CASE("Auction fixture replies populate each read topic and exact mutations [auction][state]")
{
    Session session;
    REQUIRE(RequestGoodsList({}, 1));
    ReceiveLatest("auction-goods-list");
    CHECK(GetState().Goods().totalCount == 27);
    REQUIRE(CheckSell(12));
    ReceiveLatest("auction-sell-check-result");
    REQUIRE(GetState().LastSellCheck().has_value());
    CHECK(GetState().LastSellCheck()->available == 18);
    REQUIRE(CreateListing(12, 18, 1000000, 24, 181000));
    ReceiveLatest("auction-create-listing-result");
    CHECK(GetState().LastOutcome(Operation::CreateListing)->postingFee == 181000);
    REQUIRE(RequestOwnListings());
    ReceiveLatest("auction-own-listings");
    CHECK(GetState().OwnListings().size() == 2);
    CHECK(GetState().GetRules().activeListings == 2);
    REQUIRE(CancelListing(Id(0x20)));
    ReceiveLatest("auction-cancel-listing-result");
    CHECK(GetState().LastOutcome(Operation::CancelListing)->kept == 8);
    REQUIRE(RequestCollection(0));
    ReceiveLatest("auction-collection");
    CHECK(GetState().Collection().rows.size() == 3);
    REQUIRE(Claim({}));
    ReceiveLatest("auction-claim-result");
    const auto claim = GetState().LastOutcome(Operation::Claim);
    REQUIRE(claim.has_value());
    CHECK(claim->result == Result::Partial);
    CHECK(claim->zenCollected == 1000000000);
    CHECK(claim->keptBack == 1);
    CHECK(GetState().GetRules().pendingCollection == 1);
}

TEST_CASE("Auction reset clears data and pending operations without reusing ids [auction][state]")
{
    Session session;
    REQUIRE(RequestSearch({}, 0));
    const auto oldId = LastId();
    Reset();
    CHECK_FALSE(GetState().IsOpen());
    CHECK_FALSE(GetState().IsPending(Operation::Search));
    CHECK(GetState().Search().rows.empty());
    CHECK(GetState().Notices().empty());
    CHECK_FALSE(GetState().LastOutcome(Operation::Search).has_value());
    ReceivePacket(Load("auction-house-state-open"));
    REQUIRE(RequestSearch({}, 0));
    CHECK(LastId() != oldId);
    const auto revision = GetState().Revision();
    ReceivePacket(ReplyWithId("auction-search-result", oldId));
    CHECK(GetState().Revision() == revision);
    Close();
    CHECK(closeCount == 1);
    CHECK_FALSE(GetState().IsOpen());
    Close();
    CHECK(closeCount == 1);
}

TEST_CASE("Auction not-open reply closes and malformed replies preserve pending state [auction][state]")
{
    Session session;
    REQUIRE(RequestSearch({}, 0));
    auto reply = ReplyWithId("auction-search-result-empty", LastId());
    auto malformed = reply;
    malformed.pop_back();
    const auto revision = GetState().Revision();
    ReceivePacket(malformed);
    CHECK(GetState().Revision() == revision);
    CHECK(GetState().IsPending(Operation::Search));
    reply[7] = static_cast<std::uint8_t>(Result::NotOpen);
    ReceivePacket(reply);
    CHECK_FALSE(GetState().IsOpen());
    CHECK(session.listener.closed == 1);
}

TEST_CASE("Auction refuses invalid amounts durations filters and unavailable sends [auction][state]")
{
    Session session;
    CHECK_FALSE(BuyListing({}, 100));
    CHECK_FALSE(BuyListing(Id(0), 0));
    CHECK_FALSE(BuyListing(Id(0), 2000000001));
    CHECK_FALSE(BuyGoods(7181, 0, 0, 100));
    CHECK_FALSE(RequestQuote(9000, 0, 1));
    CHECK_FALSE(CreateListing(12, 18, 1000000, 10, 181000));
    CHECK_FALSE(CreateListing(12, 18, 1000000, 24, 181001));
    SearchFilter filter;
    filter.entries.resize(49);
    CHECK_FALSE(RequestSearch(filter, 0));
    CHECK(sent.empty());
    transportReady = false;
    CHECK_FALSE(RequestSearch({}, 0));
    CHECK_FALSE(GetState().IsPending(Operation::Search));
}

TEST_CASE("Auction fees use floor rounding and reject overflow before multiplication [auction][fees]")
{
    const auto rules = AuctionFixture::Decode<RulesUpdate>("auction-house-state-open").rules;
    CHECK(PostingFee(rules, 18, 1000000) == 181000);
    CHECK(PostingFee(rules, 1, 199) == 1001);
    CHECK(PostingFee(rules, 1, rules.maxPrice) == 20001000);
    CHECK_FALSE(PostingFee(rules, 1, rules.maxPrice + 1).has_value());
    CHECK_FALSE(PostingFee(rules, std::numeric_limits<std::uint32_t>::max(),
                           std::numeric_limits<Zen>::max()).has_value());
    CHECK_FALSE(PostingFee(rules, 0, 1).has_value());
    CHECK(SaleFee(rules, 19) == 0);
    CHECK(SaleFee(rules, 8000000) == 400000);
    Rules capRules = rules;
    capRules.maxPrice = capRules.zenLimit;
    capRules.postingFeeBasisPoints = 0;
    CHECK(PostingFee(capRules, 1, 2147483647) == 1000);
    capRules.postingFeeFlat = capRules.maxPrice;
    capRules.postingFeeBasisPoints = 1;
    CHECK_FALSE(PostingFee(capRules, 1, 10000).has_value());
}

TEST_CASE("Auction countdown uses receive time and saturates at zero [auction][state]")
{
    const auto then = std::chrono::steady_clock::now();
    CHECK(SecondsRemaining(60, then, then + std::chrono::seconds(5)) == 55);
    CHECK(SecondsRemaining(60, then, then + std::chrono::seconds(61)) == 0);
    CHECK(SecondsRemaining(60, then, then - std::chrono::seconds(1)) == 60);
}

TEST_CASE("Auction goods steps cross decimal boundaries and preserve exact prices [auction][prices]")
{
    Rules rules;
    rules.maxPrice = 2000000000;
    rules.goodsPriceDigits = 3;
    CHECK(GoodsPriceOnStep(rules, 2450000));
    CHECK(GoodsPriceOnStep(rules, 999000));
    CHECK_FALSE(GoodsPriceOnStep(rules, 2449999));
    CHECK_FALSE(GoodsPriceOnStep(rules, 0));
    CHECK(PreviousGoodsPrice(rules, 2450000) == 2440000);
    CHECK(NextGoodsPrice(rules, 2450000) == 2460000);
    CHECK(PreviousGoodsPrice(rules, 1000000) == 999000);
    CHECK(NextGoodsPrice(rules, 999000) == 1000000);
    CHECK(PreviousGoodsPrice(rules, 2449999) == 2440000);
    CHECK(NextGoodsPrice(rules, 2449999) == 2450000);
    CHECK(PreviousGoodsPrice(rules, 1000) == 999);
    CHECK(NextGoodsPrice(rules, 1000) == 1010);
    CHECK_FALSE(PreviousGoodsPrice(rules, 1).has_value());
    CHECK_FALSE(NextGoodsPrice(rules, rules.maxPrice).has_value());
    CHECK_FALSE(NextGoodsPrice(rules, 0).has_value());
    rules.goodsPriceDigits = 1;
    CHECK(PreviousGoodsPrice(rules, 1000) == 900);
    CHECK(NextGoodsPrice(rules, 1000) == 2000);
    rules.goodsPriceDigits = 0;
    CHECK(GoodsPriceOnStep(rules, 2449999));
    CHECK(PreviousGoodsPrice(rules, 2450000) == 2449999);
    CHECK(NextGoodsPrice(rules, 2450000) == 2450001);
    rules.maxPrice = std::numeric_limits<Zen>::max();
    CHECK_FALSE(NextGoodsPrice(rules, rules.maxPrice).has_value());
    rules.goodsPriceDigits = 3;
    CHECK_FALSE(NextGoodsPrice(rules, rules.maxPrice - 1).has_value());
    CHECK(PreviousGoodsPrice(rules, rules.maxPrice) == 4290000000u);
    rules.goodsPriceDigits = std::numeric_limits<std::uint8_t>::max();
    CHECK(GoodsPriceOnStep(rules, rules.maxPrice));
}

TEST_CASE("Auction goods listing rejects off-step units but leaves items and totals unrestricted [auction][state]")
{
    Session session;
    REQUIRE(CheckSell(12));
    ReceiveLatest("auction-sell-check-result");
    const auto& rules = GetState().GetRules();
    REQUIRE(rules.goodsPriceDigits == 3);
    const auto fee = PostingFee(rules, 1, 2449999);
    REQUIRE(fee.has_value());
    const auto before = sent.size();
    CHECK_FALSE(CreateListing(12, 1, 2449999, rules.durations.front(), *fee));
    CHECK(sent.size() == before);
    CHECK_FALSE(GetState().IsPending(Operation::CreateListing));

    REQUIRE(CheckSell(12));
    auto item = ReplyWithId("auction-sell-check-result", LastId());
    item[9] = 1; // Server classifies this slot as an individual item.
    ReceivePacket(item);
    REQUIRE(CreateListing(12, 1, 2449999, rules.durations.front(), *fee));
    auto refused = ReplyWithId("auction-create-listing-result", LastId());
    refused[6] = static_cast<std::uint8_t>(Result::PriceNotOnStep);
    std::fill(refused.begin() + 7, refused.end(), 0);
    ReceivePacket(refused);
    CHECK_FALSE(GetState().IsPending(Operation::CreateListing));
    REQUIRE(GetState().LastOutcome(Operation::CreateListing).has_value());
    CHECK(GetState().LastOutcome(Operation::CreateListing)->result == Result::PriceNotOnStep);

    REQUIRE(CheckSell(12));
    ReceiveLatest("auction-sell-check-result");
    const auto totalFee = PostingFee(rules, 7, 999000);
    REQUIRE(totalFee.has_value());
    REQUIRE(CreateListing(12, 7, 999000, rules.durations.front(), *totalFee));
    CHECK_FALSE(GoodsPriceOnStep(rules, 7 * 999000)); // Only the unit price is restricted.
}

TEST_CASE("Auction inventory notices remain visible while the broker is closed [auction][state]")
{
    Session session;
    ReceivePacket(Load("auction-house-state-closed"));
    auto notice = Load("auction-notice-sold");
    notice[4] = static_cast<std::uint8_t>(NoticeKind::SoldToInventory);
    ReceivePacket(notice);
    notice[4] = static_cast<std::uint8_t>(NoticeKind::ExpiredToInventory);
    std::fill(notice.begin() + 12, notice.begin() + 16, 0);
    ReceivePacket(notice);
    REQUIRE(session.listener.notices.size() == 2);
    CHECK(session.listener.notices[0].kind == NoticeKind::SoldToInventory);
    CHECK(session.listener.notices[1].kind == NoticeKind::ExpiredToInventory);
    CHECK_FALSE(GetState().IsOpen());
    CHECK(GetState().GetRules().pendingCollection == 2);
}

TEST_CASE("Auction names narrow by slots and groups and never silently truncate [auction][names]")
{
    const std::array<ItemName, 4> names{{{5, 1, L"Blade"}, {26, 1, L"Crystal Sword"},
        {4105, 1u << 3, L"Dragon Armor"}, {7181, 0, L"Jewel of Bless"}}};
    std::vector<std::uint16_t> matches;
    CHECK(ResolveNames(L"bLaD", 0, 0, names, matches) == NameMatch::Ok);
    CHECK(matches == std::vector<std::uint16_t>{5});
    CHECK(ResolveNames(L"dragon", 1, 0, names, matches) == NameMatch::NoMatch);
    CHECK(matches.empty());
    CHECK(ResolveNames(L"", 0, 1u << 8, names, matches) == NameMatch::Ok);
    CHECK(matches == std::vector<std::uint16_t>{4105});
    CHECK(ResolveNames(L"absent", 0, 0, names, matches) == NameMatch::NoMatch);
    std::vector<ItemName> broad;
    broad.reserve(49);
    for (std::uint16_t i = 0; i < 49; ++i) broad.push_back({i, 1, L"Sword"});
    CHECK(ResolveNames(L"sword", 1, 0, broad, matches) == NameMatch::TooBroad);
    CHECK(matches.empty());
    broad.pop_back();
    CHECK(ResolveNames(L"sword", 1, 0, broad, matches) == NameMatch::Ok);
    CHECK(matches.size() == 48);
}
