#include <doctest.h>
#include "GameLogic/AuctionHouse/AuctionHousePriceMemory.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

using namespace GameLogic::AuctionHouse;

namespace
{
class TemporaryDirectory
{
public:
    TemporaryDirectory()
        : path(std::filesystem::temp_directory_path() /
               ("auction-price-memory-" + std::to_string(reinterpret_cast<std::uintptr_t>(this))))
    {
        std::filesystem::remove_all(path);
    }
    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    std::filesystem::path path;
};

PricingIdentity DragonArmor(std::uint8_t excellentFlags)
{
    PricingIdentity identity;
    identity.type = 4105;
    identity.level = 9;
    identity.excellentFlags = excellentFlags;
    return identity;
}
} // namespace

TEST_CASE("Auction pricing identities separate goods levels and equipment options [auction][price-memory]")
{
    CHECK(GoodsPricingIdentity(7181, 0) == GoodsPricingIdentity(7181, 0));
    CHECK_FALSE(GoodsPricingIdentity(7181, 0) == GoodsPricingIdentity(7181, 1)); // Loose jewels and bundles.
    CHECK_FALSE(GoodsPricingIdentity(7181, 0) == GoodsPricingIdentity(7182, 0));
    CHECK(DragonArmor(0x03) == DragonArmor(0x03));
    CHECK_FALSE(DragonArmor(0x03) == DragonArmor(0x05));
    auto socketed = DragonArmor(0x03);
    socketed.socketCount = 2;
    socketed.sockets[0] = 17;
    CHECK_FALSE(socketed == DragonArmor(0x03));
    auto equipmentLikeGoods = GoodsPricingIdentity(4105, 9);
    equipmentLikeGoods.goods = false;
    CHECK(equipmentLikeGoods == DragonArmor(0));
    CHECK_FALSE(GoodsPricingIdentity(4105, 9) == DragonArmor(0));
}

TEST_CASE("Auction price memory remembers forgets and drops the least recently used [auction][price-memory]")
{
    PriceMemory memory;
    const auto bless = GoodsPricingIdentity(7181, 0);
    CHECK_FALSE(memory.Find(bless));
    memory.Remember(bless, 600000);
    memory.Remember(bless, 590000);
    CHECK(memory.Find(bless) == 590000);
    CHECK(memory.Entries().size() == 1);
    CHECK_FALSE(memory.Find(DragonArmor(0x05)));
    memory.Remember(DragonArmor(0x03), 12500000);
    CHECK_FALSE(memory.Find(DragonArmor(0x05)));
    CHECK(memory.Forget(DragonArmor(0x03)));
    CHECK_FALSE(memory.Forget(DragonArmor(0x03)));
    CHECK_FALSE(memory.Find(DragonArmor(0x03)));

    for (std::uint16_t type = 1; type <= PriceMemory::Capacity; ++type)
        memory.Remember(GoodsPricingIdentity(type, 0), type);
    CHECK(memory.Entries().size() == PriceMemory::Capacity);
    CHECK_FALSE(memory.Find(bless)); // Used longest ago.
    CHECK(memory.Find(GoodsPricingIdentity(1, 0)) == 1u);
    memory.Remember(GoodsPricingIdentity(1, 0), 5); // Publishing again makes it the most recent.
    memory.Remember(bless, 590000);
    CHECK(memory.Find(GoodsPricingIdentity(1, 0)) == 5u);
    CHECK_FALSE(memory.Find(GoodsPricingIdentity(2, 0)));
    CHECK(memory.Entries().front().identity == bless);

    // A prefill from the remembered price is a use: the oldest entry, used now, outlives the next one.
    CHECK(memory.Entries().back().identity == GoodsPricingIdentity(3, 0));
    CHECK(memory.Use(GoodsPricingIdentity(3, 0)) == 3u);
    CHECK(memory.Entries().front().identity == GoodsPricingIdentity(3, 0));
    memory.Remember(DragonArmor(0x05), 7000000);
    CHECK(memory.Find(GoodsPricingIdentity(3, 0)) == 3u);
    CHECK_FALSE(memory.Find(GoodsPricingIdentity(4, 0)));
    // Find (the Forget link, the field label) is not a use.
    const auto last = memory.Entries().back().identity;
    CHECK(memory.Find(last));
    CHECK(memory.Entries().back().identity == last);
    CHECK_FALSE(memory.Use(GoodsPricingIdentity(4, 0)));
    CHECK(memory.Entries().size() == PriceMemory::Capacity);
}

TEST_CASE("Auction price memory saves the use order with the next write [auction][price-memory]")
{
    TemporaryDirectory directory;
    const auto owner = PriceMemoryOwner("127.126.3.1", 44405, "Mutilate", 1, "seller");
    const auto path = directory.path / PriceMemoryPath(owner).filename();
    PriceMemory memory;
    memory.Remember(GoodsPricingIdentity(7181, 0), 590000);
    memory.Remember(GoodsPricingIdentity(7182, 0), 1200000);
    REQUIRE(memory.Save(path, owner));
    const auto written = std::filesystem::last_write_time(path);
    REQUIRE(memory.Use(GoodsPricingIdentity(7181, 0)));
    // No write on use.
    CHECK(std::filesystem::last_write_time(path) == written);
    PriceMemory before;
    REQUIRE(before.Load(path, owner));
    CHECK(before.Entries().front().identity == GoodsPricingIdentity(7182, 0));
    REQUIRE(memory.Save(path, owner));
    PriceMemory after;
    REQUIRE(after.Load(path, owner));
    CHECK(after.Entries().front().identity == GoodsPricingIdentity(7181, 0));
}

TEST_CASE("Auction price memory bounds a hostile file: line length, record count and repeated identities [auction][price-memory]")
{
    TemporaryDirectory directory;
    std::filesystem::create_directories(directory.path);
    const auto owner = PriceMemoryOwner("127.126.3.1", 44405, "Mutilate", 1, "seller");
    const auto path = directory.path / PriceMemoryPath(owner).filename();
    std::string header;
    {
        PriceMemory empty;
        REQUIRE(empty.Save(path, owner));
        std::ifstream in(path);
        std::string version, hex;
        std::getline(in, version);
        std::getline(in, hex);
        header = version + "\n" + hex + "\n";
    }
    auto entryLine = [](std::uint16_t type, Zen price)
    { return std::to_string(price) + " 1 " + std::to_string(type) + " 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"; };
    auto write = [&path, &header](const std::string& body) { std::ofstream(path, std::ios::trunc) << header << body; };

    // An over-long line, entry or header, refuses the whole file without reading it into memory.
    write(entryLine(7181, 590000) + std::string(PriceMemory::MaximumLineLength + 1, '1') + "\n");
    PriceMemory longLine;
    CHECK_FALSE(longLine.Load(path, owner));
    CHECK(longLine.Entries().empty());
    std::ofstream(path, std::ios::trunc) << "1\n" << std::string(10 * PriceMemory::MaximumLineLength, 'a') << "\n";
    CHECK_FALSE(longLine.Load(path, owner));
    // A line of exactly the maximum length is still read (and here refused as no entry).
    write(std::string(PriceMemory::MaximumLineLength, '1') + "\n");
    CHECK_FALSE(longLine.Load(path, owner));

    // A repeated identity keeps its first, most recent line.
    write(entryLine(7181, 590000) + entryLine(7182, 1200000) + entryLine(7181, 1));
    PriceMemory repeated;
    REQUIRE(repeated.Load(path, owner));
    CHECK(repeated.Entries().size() == 2);
    CHECK(repeated.Find(GoodsPricingIdentity(7181, 0)) == 590000u);

    // Endless repeats stop after Capacity lines: the reading work is bounded, not the file size.
    std::string many;
    for (int i = 0; i < 10000; ++i)
        many += entryLine(7181, 590000);
    write(many + "not a price\n");
    PriceMemory flooded;
    REQUIRE(flooded.Load(path, owner));
    CHECK(flooded.Entries().size() == 1);

    // More distinct records than Capacity: the first Capacity are kept, the rest is never read.
    std::string full;
    for (std::uint16_t type = 1; type <= PriceMemory::Capacity + 10; ++type)
        full += entryLine(type, type);
    write(full + "not a price\n");
    PriceMemory capped;
    REQUIRE(capped.Load(path, owner));
    CHECK(capped.Entries().size() == PriceMemory::Capacity);
    CHECK(capped.Find(GoodsPricingIdentity(1, 0)) == 1u);
    CHECK_FALSE(capped.Find(GoodsPricingIdentity(PriceMemory::Capacity + 1, 0)));
}

TEST_CASE("Auction price memory survives a restart for its owner only [auction][price-memory]")
{
    TemporaryDirectory directory;
    const auto owner = PriceMemoryOwner("127.126.3.1", 44405, "Mutilate", 1, "seller");
    const auto otherAccount = PriceMemoryOwner("127.126.3.1", 44405, "Mutilate", 1, "buyer");
    const auto otherServer = PriceMemoryOwner("127.126.3.1", 44405, "Mutilate", 2, "seller");
    CHECK(owner != otherAccount);
    CHECK(owner != otherServer);
    CHECK(PriceMemoryOwner("a", 1, "bc", 1, "d") != PriceMemoryOwner("a", 1, "b", 1, "cd"));
    CHECK(PriceMemoryPath(owner) != PriceMemoryPath(otherAccount));
    CHECK(PriceMemoryPath(owner) == PriceMemoryPath(owner));
    CHECK(PriceMemoryPath(owner).parent_path() == std::filesystem::path("Config/auction-prices"));
    CHECK(PriceMemoryPath(owner).string().find("seller") == std::string::npos);

    const auto path = directory.path / PriceMemoryPath(owner).filename();
    auto armor = DragonArmor(0x03);
    armor.luck = true;
    armor.harmonyOption = 3;
    armor.harmonyLevel = 7;
    armor.sockets = {1, 2, 3, 4, 5};
    armor.option380 = true;
    {
        PriceMemory memory;
        memory.Remember(GoodsPricingIdentity(7181, 0), 590000);
        memory.Remember(armor, 12500000);
        REQUIRE(memory.Save(path, owner));
    }
    PriceMemory restored;
    REQUIRE(restored.Load(path, owner));
    CHECK(restored.Find(GoodsPricingIdentity(7181, 0)) == 590000);
    CHECK(restored.Find(armor) == 12500000);
    CHECK(restored.Entries().front().identity == armor);

    PriceMemory other;
    CHECK_FALSE(other.Load(path, otherAccount));
    CHECK(other.Entries().empty());
    CHECK_FALSE(other.Load(directory.path / "missing.prices", owner));

    std::ofstream(path, std::ios::app) << "not a price\n";
    PriceMemory damaged;
    CHECK_FALSE(damaged.Load(path, owner));
    CHECK(damaged.Entries().empty());
}

TEST_CASE("Auction remembered prices from two clients of one account are both kept [auction][price-memory]")
{
    // FR-02: two clients load the same store; each publishes from its own, by then stale, copy.
    TemporaryDirectory directory;
    const auto owner = PriceMemoryOwner("127.126.3.1", 44405, "Mutilate", 1, "seller");
    const auto path = directory.path / PriceMemoryPath(owner).filename();
    const auto bless = GoodsPricingIdentity(7181, 0), soul = GoodsPricingIdentity(7182, 0),
               chaos = GoodsPricingIdentity(6159, 0);
    {
        PriceMemory seed;
        seed.Remember(chaos, 2000000);
        REQUIRE(seed.Save(path, owner));
    }
    PriceMemory first, second;
    REQUIRE(first.Load(path, owner));
    REQUIRE(second.Load(path, owner));
    REQUIRE(first.SavePublished(path, owner, bless, 590000));
    REQUIRE(second.SavePublished(path, owner, soul, 1200000));
    PriceMemory saved;
    REQUIRE(saved.Load(path, owner));
    CHECK(saved.Find(bless) == 590000u);
    CHECK(saved.Find(soul) == 1200000u);
    CHECK(saved.Find(chaos) == 2000000u);
    // The writer's own copy is what it wrote, the other client's publication included.
    CHECK(second.Find(bless) == 590000u);
    CHECK(saved.Entries().front().identity == soul);

    // Forget in one client is not undone by the other's next publication from its stale copy.
    REQUIRE(first.SaveForgotten(path, owner, chaos));
    REQUIRE(second.Find(chaos));
    REQUIRE(second.SavePublished(path, owner, bless, 595000));
    REQUIRE(saved.Load(path, owner));
    CHECK_FALSE(saved.Find(chaos));
    CHECK(saved.Find(bless) == 595000u);
    CHECK(saved.Find(soul) == 1200000u);

    // A use in one client is carried over by its next update; the file holds the merged order.
    REQUIRE(first.Load(path, owner));
    REQUIRE(first.Use(soul));
    REQUIRE(first.SavePublished(path, owner, chaos, 2100000));
    REQUIRE(saved.Load(path, owner));
    REQUIRE(saved.Entries().size() == 3);
    CHECK(saved.Entries()[0].identity == chaos);
    CHECK(saved.Entries()[1].identity == soul);
    CHECK(saved.Entries()[2].identity == bless);
}

TEST_CASE("Auction remembered-price writers never share a temporary file [auction][price-memory]")
{
    TemporaryDirectory directory;
    const auto owner = PriceMemoryOwner("127.126.3.1", 44405, "Mutilate", 1, "seller");
    const auto path = directory.path / PriceMemoryPath(owner).filename();
    CHECK(UniqueTemporaryPath(path) != UniqueTemporaryPath(path));
    CHECK(UniqueTemporaryPath(path).parent_path() == path.parent_path());

    // Overlapping updates from separate stores (separate lock handles): none is lost.
    constexpr int Writers = 4, Publications = 25;
    std::atomic<int> failed{0};
    std::vector<std::thread> threads;
    threads.reserve(Writers);
    for (int writer = 0; writer < Writers; ++writer)
        threads.emplace_back(
            [&path, &owner, &failed, writer]
            {
                PriceMemory store;
                store.Load(path, owner);
                for (int n = 0; n < Publications; ++n)
                    if (!store.SavePublished(path, owner,
                                             GoodsPricingIdentity(static_cast<std::uint16_t>(writer * 100 + n), 0),
                                             static_cast<Zen>(1000 + n)))
                        ++failed;
            });
    for (auto& thread : threads)
        thread.join();
    CHECK(failed == 0);
    PriceMemory saved;
    REQUIRE(saved.Load(path, owner));
    CHECK(saved.Entries().size() == Writers * Publications);
    // No temporary file is left behind; only the store and its lock file remain.
    for (const auto& entry : std::filesystem::directory_iterator(directory.path))
        CHECK(entry.path().filename().string().find(".tmp") == std::string::npos);
}
