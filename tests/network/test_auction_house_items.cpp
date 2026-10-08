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
    CHECK(UI::AuctionHouse::SameInventoryItem(&item, &saved, saved.Key));
    item.Durability = 39;
    CHECK_FALSE(UI::AuctionHouse::SameInventoryItem(&item, &saved, saved.Key));
    CHECK(UI::AuctionHouse::DescribeItem(&item, 12).detail == "Durability 39/40");
    saved = item;
    item.bySocketOption[0] = 2;
    CHECK_FALSE(UI::AuctionHouse::SameInventoryItem(&item, &saved, saved.Key));
    saved = item;
    item.Jewel_Of_Harmony_Option = 1;
    CHECK_FALSE(UI::AuctionHouse::SameInventoryItem(&item, &saved, saved.Key));
    CHECK_FALSE(UI::AuctionHouse::SameInventoryItem(nullptr, &saved, saved.Key));
}

TEST_CASE("Auction preview copies retain sale identity separately [auction][items]")
{
    ItemCatalogue catalogue;
    mu::ui::window::CItemMng manager;
    auto bytes = AuctionFixture::Decode<GameLogic::AuctionHouse::SearchPage>("auction-search-result").rows[0].itemData;
    auto* item = manager.CreateItem(bytes);
    REQUIRE(item != nullptr);
    const auto sourceKey = item->Key;
    auto* preview = manager.DuplicateItem(item);
    REQUIRE(preview != nullptr);
    CHECK(preview->Key != sourceKey);
    CHECK(UI::AuctionHouse::SameInventoryItem(item, preview, sourceKey));
    item->Key++;
    CHECK_FALSE(UI::AuctionHouse::SameInventoryItem(item, preview, sourceKey));
}

TEST_CASE("Auction equipment pricing identity ignores serial slot and durability [auction][items]")
{
    ItemCatalogue catalogue;
    mu::ui::window::CItemMng manager;
    auto bytes = AuctionFixture::Decode<GameLogic::AuctionHouse::SearchPage>("auction-search-result").rows[0].itemData;
    auto* item = manager.CreateItem(bytes);
    REQUIRE(item != nullptr);
    auto* copy = manager.DuplicateItem(item);
    REQUIRE(copy != nullptr);
    copy->Durability = static_cast<BYTE>(item->Durability - 1);
    copy->x = static_cast<BYTE>(item->x + 1);
    copy->bySelectedSlotIndex = static_cast<BYTE>(item->bySelectedSlotIndex + 1);
    const auto identity = UI::AuctionHouse::EquipmentPricingIdentity(*item);
    CHECK_FALSE(identity.goods);
    CHECK(identity.type == 5);
    CHECK(identity.level == 9);
    CHECK(identity.excellentFlags == 9);
    CHECK(identity.luck);
    CHECK(identity.skill);
    CHECK(identity.optionLevel == 3);
    CHECK(UI::AuctionHouse::EquipmentPricingIdentity(*copy) == identity);
    const auto changed = [&](auto change)
    {
        auto other = *copy;
        change(other);
        return !(UI::AuctionHouse::EquipmentPricingIdentity(other) == identity);
    };
    CHECK(changed([](ITEM& other) { other.Level++; }));
    CHECK(changed([](ITEM& other) { other.ExcellentFlags ^= 2; }));
    CHECK(changed([](ITEM& other) { other.HasLuck = false; }));
    CHECK(changed([](ITEM& other) { other.HasSkill = false; }));
    CHECK(changed([](ITEM& other) { other.OptionLevel++; }));
    CHECK(changed([](ITEM& other) { other.AncientDiscriminator = 2; }));
    CHECK(changed([](ITEM& other) { other.AncientBonusOption = 10; }));
    CHECK(changed([](ITEM& other) { other.SocketCount = 1; }));
    CHECK(changed([](ITEM& other) { other.bySocketOption[0] = 7; }));
    CHECK(changed([](ITEM& other) { other.Jewel_Of_Harmony_Option = 2; }));
    CHECK(changed([](ITEM& other) { other.Jewel_Of_Harmony_OptionLevel = 4; }));
    CHECK(changed([](ITEM& other) { other.option_380 = !other.option_380; }));
}

TEST_CASE("Auction rows keep the item's excellent options and Skill applies where an item carries one [auction][items]")
{
    ItemCatalogue catalogue;
    std::wcscpy(ItemAttribute[5].Name, L"Blade");
    ItemAttribute[5].m_byItemSlot = EQUIPMENT_WEAPON_RIGHT;
    ItemAttribute[5].Durability = 40;
    std::wcscpy(ItemAttribute[4105].Name, L"Dragon Armor");
    ItemAttribute[4105].m_byItemSlot = EQUIPMENT_ARMOR;
    CHECK(GameLogic::AuctionHouse::ItemSlotMask(5) == 1u << 0);
    CHECK(GameLogic::AuctionHouse::ItemSlotMask(4105) == 1u << 3);
    CHECK_FALSE(GameLogic::AuctionHouse::SkillApplies(0, 0));
    ItemAttribute[5].m_wSkillIndex = 1;
    CHECK(GameLogic::AuctionHouse::SkillApplies(0, 0));
    CHECK(GameLogic::AuctionHouse::SkillApplies(1u << 0, 0));
    CHECK_FALSE(GameLogic::AuctionHouse::SkillApplies(1u << 3, 0));
    CHECK_FALSE(GameLogic::AuctionHouse::SkillApplies(1u << 0, 1u << 1));

    ITEM item{};
    item.Type = 5;
    item.Durability = 40;
    item.ExcellentFlags = 0x09;
    auto row = UI::AuctionHouse::DescribeItem(&item, 0);
    UI::AuctionHouse::MarkSearchedOptions(row.excellent, 0x08);
    CHECK(row.name == "Excellent Blade");
    REQUIRE(row.excellent.size() == 2);
    CHECK(row.excellent[0].label == "Damage +2%");
    CHECK(row.excellent[0].matched);
    CHECK(row.excellent[1].label == "Mana on kill");
    CHECK_FALSE(row.excellent[1].matched);
}
