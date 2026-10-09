#include <doctest.h>
#include "UI/AuctionHouse/Presentation.h"
#include <limits>

using namespace UI::AuctionHouse;

TEST_CASE("Auction Zen entry remains exact at limits and decimal shorthand [auction][ui]")
{
    constexpr auto limit = std::numeric_limits<Market::Zen>::max();
    CHECK(ParseZen("1.35kk", limit) == 1350000);
    CHECK(ParseZen(" 4,294,967,295 ", limit) == limit);
    CHECK(ParseZen("2.3b", limit) == 2300000000u);
    CHECK(ParseZen("1.01k", limit) == 1010);
    CHECK_FALSE(ParseZen("1.001k", limit));
    CHECK_FALSE(ParseZen("1.000kk", limit));
    CHECK_FALSE(ParseZen("1.234k", limit));
    CHECK_FALSE(ParseZen("4.294967295B", limit));
    CHECK(ParseZen("999", 999) == 999);
    CHECK(ParseZen("1,000", limit) == 1000);
    CHECK(ParseZen("1.5 kk", limit) == 1500000);
    CHECK(ParseZen("1 kk", limit) == 1000000);
    CHECK(ParseQuantity("25", 25) == 25);
    CHECK_FALSE(ParseQuantity("26", 25));
    CHECK_FALSE(ParseQuantity("1k", 1000));
    CHECK_FALSE(ParseQuantity("1.0", 2));
    CHECK_FALSE(ParseZen("1000", 999));
    for (const char* input :
         {"", "-1", "+1", "12,34", "1.000", "1,5M", "1.0001k", "1e6", "1.", "4294967296", "999999999999999999999B"})
    {
        INFO(input);
        CHECK_FALSE(ParseZen(input, limit));
    }
    CHECK(ExactZen(limit) == "4,294,967,295");
    CHECK(CompactZen(1350000000) == "1,350kk");
    CHECK(CompactZen(12500000) == "12.5kk");
    CHECK(ParseZen(CompactZen(4294967295), limit) == limit);
    CHECK(CompactZen(1000001) == "1,000,001");
}

TEST_CASE("Auction balance is always compact and never overstated [auction][ui]")
{
    CHECK(BalanceZen(28470000) == "28.47kk");
    CHECK(BalanceZen(19823000) == "19.82kk");
    CHECK(BalanceZen(45789999) == "45.78kk");
    CHECK(BalanceZen(60000000) == "60kk");
    CHECK(BalanceZen(1000000) == "1kk");
    CHECK(BalanceZen(999999) == "999.99k");
    CHECK(BalanceZen(590000) == "590k");
    CHECK(BalanceZen(1234) == "1.23k");
    CHECK(BalanceZen(1000) == "1k");
    CHECK(BalanceZen(999) == "999");
    CHECK(BalanceZen(0) == "0");
    CHECK(BalanceZen(2000000000) == "2,000kk");
    CHECK(BalanceZen(4294967295) == "4,294.96kk");
}

TEST_CASE("Auction parameter search intersects excellent option domains [auction][ui]")
{
    Market::SearchFilter all;
    REQUIRE(ApplyEquipmentFilter(all, Equipment::All, 1));
    CHECK(all.excellentMask == 0x02); // Native armor bit 1: defense success rate.
    CHECK(all.slotMask == ((1 << 1) | (0x1f << 2) | (1 << 9)));
    CHECK(all.family == 2);
    Market::SearchFilter weapon;
    weapon.slotMask = 1 << 0;
    CHECK_FALSE(ApplyEquipmentFilter(weapon, Equipment::Weapons, 1));
    Market::SearchFilter rings;
    REQUIRE(ApplyEquipmentFilter(rings, Equipment::Accessories, 1));
    CHECK(rings.slotMask == (1 << 9));
    Market::SearchFilter pendant;
    REQUIRE(ApplyEquipmentFilter(pendant, Equipment::Accessories, 7));
    CHECK(pendant.slotMask == (1 << 8));
    CHECK(pendant.excellentMask == 0x20);
    Market::SearchFilter gloves;
    gloves.slotMask = 1 << 5;
    REQUIRE(ApplyEquipmentFilter(gloves, Equipment::Armor, 1));
    CHECK(gloves.slotMask == (1 << 5));
    CHECK_FALSE(ApplyEquipmentFilter(gloves, Equipment::All, 99));
}

TEST_CASE("Auction aggregate request clock stays below broker rate limit [auction][ui]")
{
    RequestClock clock;
    const auto start = RequestClock::Time{} + std::chrono::seconds(10);
    int sent = 0;
    for (int ms = 0; ms < 1000; ++ms)
    {
        const auto now = start + std::chrono::milliseconds(ms);
        if (!clock.Ready(now))
            continue;
        clock.Sent(now);
        ++sent;
    }
    CHECK(sent == 4);
    clock.Busy(start + std::chrono::seconds(1));
    CHECK_FALSE(clock.Ready(start + std::chrono::milliseconds(1999)));
    CHECK(clock.Ready(start + std::chrono::seconds(2)));
    clock.Reset();
    CHECK(clock.Ready(start));
}

TEST_CASE("Auction typed properties compose with names and explicit families [auction][ui]")
{
    Market::SearchFilter filter;
    REQUIRE(ApplyEquipmentFilter(filter, Equipment::All, 1));
    std::string query = "Dragon gloves Defense success rate +10% & reflect damage Luck Skill";
    REQUIRE(ParsePropertyQuery(filter, query));
    CHECK(query == "Dragon gloves");
    CHECK(filter.excellentMask == 0x06);
    CHECK(filter.flags == 6);
    CHECK(filter.family == 2);
    Market::SearchFilter incompatible;
    REQUIRE(ApplyEquipmentFilter(incompatible, Equipment::All, 1));
    query = "Attack speed +7";
    CHECK_FALSE(ParsePropertyQuery(incompatible, query));
    Market::SearchFilter ancient;
    ancient.family = 3;
    REQUIRE(ApplyEquipmentFilter(ancient, Equipment::Armor, 0));
    query = "Luck Dragon";
    REQUIRE(ParsePropertyQuery(ancient, query));
    CHECK(ancient.family == 3);
    CHECK(query == "Dragon");
    query = "Lucky ring";
    REQUIRE(ParsePropertyQuery(ancient, query));
    CHECK(query == "Lucky ring");
}

TEST_CASE("Auction goods directory matches categories and every name term [auction][ui]")
{
    CHECK(MatchesGoodsName("Jewel of Soul", Category::Jewels, "soul jewel"));
    CHECK_FALSE(MatchesGoodsName("Jewel of Soul", Category::Materials, "soul jewel"));
    CHECK(MatchesGoodsName("Ancient Armor Piece", Category::Materials, "armor piece"));
    CHECK_FALSE(MatchesGoodsName("Jewel of Bless", Category::Jewels, "soul jewel"));
    CHECK_FALSE(MatchesGoodsName("Jewel of Soul", Category::Equipment, "soul"));
}
