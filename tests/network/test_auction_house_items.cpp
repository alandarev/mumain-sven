#include "stdafx.h"
#include "AuctionFixture.h"
#include "UI/Inventory/ItemMng.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Network/Server/WSclient.h"
#include "GameLogic/AuctionHouse/AuctionHouse.h"
#include "UI/AuctionHouse/ItemPresentation.h"

#include <memory>

int CalcItemLength(std::span<const BYTE>);

namespace
{
class ItemCatalogue
{
public:
    ItemCatalogue() : saved(ItemAttribute), attributes(std::make_unique<ITEM_ATTRIBUTE[]>(MAX_ITEM))
    {
        ItemAttribute = attributes.get();
        for (int type : {5, 26, 4105})
        {
            attributes[type].Width = 1;
            attributes[type].Height = 1;
            attributes[type].Level = 1;
        }
    }
    ~ItemCatalogue()
    {
        ItemAttribute = saved;
    }

private:
    ITEM_ATTRIBUTE* saved;
    std::unique_ptr<ITEM_ATTRIBUTE[]> attributes;
};
} // namespace

TEST_CASE("Auction BLADE ARMOR SOCK fixtures use real native item decoding [auction][items]")
{
    ItemCatalogue catalogue;
    mu::ui::window::CItemMng manager;
    const auto search = AuctionFixture::Decode<GameLogic::AuctionHouse::SearchPage>("auction-search-result");
    const auto collection = AuctionFixture::Decode<GameLogic::AuctionHouse::CollectionPage>("auction-collection");
    REQUIRE(search.rows.size() == 2);
    REQUIRE(collection.rows.size() == 3);
    const auto& bladeBytes = search.rows[0].itemData;
    const auto& armorBytes = collection.rows[0].itemData;
    const auto& socketBytes = search.rows[1].itemData;
    CHECK(CalcItemLength(bladeBytes) == 7);
    CHECK(CalcItemLength(armorBytes) == 7);
    CHECK(CalcItemLength(socketBytes) == 10);
    const auto* blade = manager.CreateItem(bladeBytes);
    const auto* armor = manager.CreateItem(armorBytes);
    const auto* sockets = manager.CreateItem(socketBytes);
    REQUIRE(blade != nullptr);
    REQUIRE(armor != nullptr);
    REQUIRE(sockets != nullptr);
    CHECK(blade->Type == 5);
    CHECK(blade->Level == 9);
    CHECK(blade->Durability == 42);
    CHECK(blade->HasLuck);
    CHECK(blade->HasSkill);
    CHECK(blade->OptionLevel == 3);
    CHECK(blade->ExcellentFlags == 9);
    CHECK(armor->Type == 4105);
    CHECK(armor->Level == 7);
    CHECK(armor->Durability == 45);
    CHECK(armor->HasLuck);
    CHECK_FALSE(armor->HasSkill);
    CHECK(armor->OptionLevel == 2);
    CHECK(armor->AncientDiscriminator == 1);
    CHECK(armor->AncientBonusOption == 5);
    CHECK(sockets->Type == 26);
    CHECK(sockets->Level == 11);
    CHECK(sockets->HasSkill);
    CHECK(sockets->OptionLevel == 1);
    CHECK(sockets->SocketCount == 3);
    CHECK(sockets->bySocketOption[0] == 1);
    CHECK(sockets->bySocketOption[1] == SOCKET_EMPTY);
    CHECK(sockets->bySocketOption[2] == SOCKET_EMPTY);
    CHECK(sockets->SocketSeedID[1] == SOCKET_EMPTY);
    CHECK(sockets->SocketSeedID[2] == SOCKET_EMPTY);
}

TEST_CASE("Auction native name search uses the loaded catalogue [auction][items]")
{
    ItemCatalogue catalogue;
    std::wcscpy(ItemAttribute[5].Name, L"Blade");
    ItemAttribute[5].m_byItemSlot = EQUIPMENT_WEAPON_RIGHT;
    std::wcscpy(ItemAttribute[4105].Name, L"Dragon Armor");
    ItemAttribute[4105].m_byItemSlot = EQUIPMENT_ARMOR;
    std::vector<std::uint16_t> matches;
    CHECK(GameLogic::AuctionHouse::ResolveItemTypes(L"BLA", 1, 1, matches) == GameLogic::AuctionHouse::NameMatch::Ok);
    CHECK(matches == std::vector<std::uint16_t>{5});
    CHECK(GameLogic::AuctionHouse::ResolveItemTypes(L"armor dragon", 1u << 3, 1u << 8, matches) ==
          GameLogic::AuctionHouse::NameMatch::Ok);
    CHECK(matches == std::vector<std::uint16_t>{4105});
}

TEST_CASE("Auction item facts stay minimal and changed inventory invalidates review [auction][items]")
{
    ItemCatalogue catalogue;
    std::wcscpy(ItemAttribute[5].Name, L"Blade");
    ItemAttribute[5].Durability = 40;
    ITEM item{};
    item.Type = 5;
    item.Durability = 40;
    item.Key = 123;
    const auto healthy = UI::AuctionHouse::DescribeItem(&item, 12);
    CHECK(healthy.name == "Blade");
    CHECK(healthy.detail.empty());
    CHECK_FALSE(healthy.cannotEquip);
    auto saved = item;
    CHECK(UI::AuctionHouse::SameInventoryItem(&item, &saved));
    item.Durability = 39;
    CHECK_FALSE(UI::AuctionHouse::SameInventoryItem(&item, &saved));
    CHECK(UI::AuctionHouse::DescribeItem(&item, 12).detail == "Durability 39/40");
    saved = item;
    item.bySocketOption[0] = 2;
    CHECK_FALSE(UI::AuctionHouse::SameInventoryItem(&item, &saved));
    saved = item;
    item.Jewel_Of_Harmony_Option = 1;
    CHECK_FALSE(UI::AuctionHouse::SameInventoryItem(&item, &saved));
    CHECK_FALSE(UI::AuctionHouse::SameInventoryItem(nullptr, &saved));
}
