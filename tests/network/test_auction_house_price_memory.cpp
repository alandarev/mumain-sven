#include <doctest.h>
#include "GameLogic/AuctionHouse/AuctionHousePriceMemory.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

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

TEST_CASE("Auction price memory remembers forgets and drops the least recently published [auction][price-memory]")
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
    CHECK_FALSE(memory.Find(bless)); // Published longest ago.
    CHECK(memory.Find(GoodsPricingIdentity(1, 0)) == 1u);
    memory.Remember(GoodsPricingIdentity(1, 0), 5); // Publishing again makes it the most recent.
    memory.Remember(bless, 590000);
    CHECK(memory.Find(GoodsPricingIdentity(1, 0)) == 5u);
    CHECK_FALSE(memory.Find(GoodsPricingIdentity(2, 0)));
    CHECK(memory.Entries().front().identity == bless);
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
