#include "stdafx.h"
#include "AuctionFixture.h"
#include "UI/Inventory/ItemMng.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Network/Server/WSclient.h"
#include "GameLogic/AuctionHouse/AuctionHouse.h"
#include "UI/AuctionHouse/ItemPresentation.h"
#include "UI/AuctionHouse/Receipts.h"
#include "GameLogic/Items/ItemPickupText.h"

#include <cwchar>
#include <memory>
#include <string>
#include <vector>

int CalcItemLength(std::span<const BYTE>);

namespace
{
class ItemCatalogue
{
public:
    ItemCatalogue() : saved(ItemAttribute), attributes(std::make_unique<ITEM_ATTRIBUTE[]>(MAX_ITEM))
    {
        ItemAttribute = attributes.get();
        for (int type : {5, 26, 4097, 4105})
        {
            attributes[type].Width = 1;
            attributes[type].Height = 1;
            attributes[type].Level = 1;
        }
        std::wcscpy(attributes[4097].Name, L"Dragon Armor");
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

TEST_CASE("Auction Sell checks its own item snapshot, not the Browse listing shown after Compare market [auction][items]")
{
    // CR-03: the Sell tab's item and the shown Browse listing are different copies; a notice or a wallet
    // change revalidates Sell against its own snapshot only.
    ItemCatalogue catalogue;
    mu::ui::window::CItemMng manager;
    const auto rows = AuctionFixture::Decode<GameLogic::AuctionHouse::SearchPage>("auction-search-result").rows;
    auto* inventory = manager.CreateItem(rows[0].itemData);
    auto* listing = manager.CreateItem(rows[1].itemData);
    REQUIRE(inventory != nullptr);
    REQUIRE(listing != nullptr);
    const auto key = inventory->Key;
    auto* sellSnapshot = manager.DuplicateItem(inventory);
    auto* shown = manager.DuplicateItem(listing);
    REQUIRE(sellSnapshot != nullptr);
    REQUIRE(shown != nullptr);
    CHECK(UI::AuctionHouse::SameInventoryItem(inventory, sellSnapshot, key));
    // The old check compared the inventory item with the shown icon and cleared the Sell selection.
    CHECK_FALSE(UI::AuctionHouse::SameInventoryItem(inventory, shown, key));
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

TEST_CASE("Auction sale notice names the sold item as its tooltip does, with Excellent [auction][items]")
{
    ItemCatalogue catalogue;
    mu::ui::window::CItemMng manager;
    const auto notice = AuctionFixture::Decode<GameLogic::AuctionHouse::Notice>("auction-notice-sold-item");
    const auto name = UI::AuctionHouse::SerializedItemName(notice.itemData, manager);
    CHECK(name == "Excellent Dragon Armor +9");
    CHECK(UI::AuctionHouse::DescribeNotice(notice, name).text ==
          "Sold Excellent Dragon Armor +9 · 24,120,000 Zen added to your bag");
    // No or undecodable data: empty, and the window falls back to the type and level name.
    CHECK(UI::AuctionHouse::SerializedItemName({}, manager).empty());
    const std::vector<std::uint8_t> truncated{0x80, 0x01, 0x09};
    CHECK(UI::AuctionHouse::SerializedItemName(truncated, manager).empty());
}

TEST_CASE("Auction Collection names the sold item of proceeds as it looks, with Excellent [auction][items]")
{
    ItemCatalogue catalogue;
    mu::ui::window::CItemMng manager;
    const auto collection =
        AuctionFixture::Decode<GameLogic::AuctionHouse::CollectionPage>("auction-collection-proceeds-item");
    REQUIRE(collection.rows.size() == 1);
    const auto& proceeds = collection.rows[0];
    const auto name = UI::AuctionHouse::SerializedItemName(proceeds.itemData, manager);
    CHECK(name == "Excellent Dragon Armor +9");
    const auto row = UI::AuctionHouse::DescribeProceedsRow(proceeds, name);
    CHECK(row.name == "Excellent Dragon Armor +9 · 24,120,000 Zen");
    CHECK(row.meta == "Sold");
    CHECK(row.exactPrice == "24,120,000 Zen");
    CHECK(row.breakdown == "Gross 26,800,000 Zen · Sale fee 2,680,000 Zen");
    // Goods and entries without a copy: the type and level name, the quantity, or "Sold item".
    auto goods = proceeds;
    goods.quantity = 8;
    CHECK(UI::AuctionHouse::DescribeProceedsRow(goods, "Jewel of Bless").name == "Jewel of Bless × 8 · 24,120,000 Zen");
    CHECK(UI::AuctionHouse::DescribeProceedsRow(proceeds, "").name == "Sold item · 24,120,000 Zen");
}

TEST_CASE("Pickup chat lines group Zen digits and name items as the tooltip does [items][chat]")
{
    ItemCatalogue catalogue;
    CHECK(GameLogic::Items::GroupedZen(0) == L"0");
    CHECK(GameLogic::Items::GroupedZen(999) == L"999");
    CHECK(GameLogic::Items::GroupedZen(1000) == L"1,000");
    CHECK(GameLogic::Items::GroupedZen(3186000) == L"3,186,000");
    CHECK(GameLogic::Items::GroupedZen(2000000000) == L"2,000,000,000");
    CHECK(GameLogic::Items::ZenObtainedText(3186000) == L"3,186,000 Zen Obtained");

    ITEM armor{};
    armor.Type = 4097;
    armor.Level = 9;
    CHECK(GameLogic::Items::ItemDisplayName(armor) == L"Dragon Armor +9");
    armor.ExcellentFlags = 0x3F;
    CHECK(GameLogic::Items::ItemDisplayName(armor) == L"Excellent Dragon Armor +9");
    CHECK(GameLogic::Items::ItemObtainedText(armor) == L"Excellent Dragon Armor +9 Obtained");
    armor.Level = 0;
    CHECK(GameLogic::Items::ItemDisplayName(armor) == L"Excellent Dragon Armor");
    // The Horn of Fenrir uses its excellent bits for its kind: never "Excellent".
    ITEM horn{};
    horn.Type = ITEM_HORN_OF_FENRIR;
    horn.ExcellentFlags = 0x01;
    CHECK(GameLogic::Items::ItemDisplayName(horn).find(L"Excellent") == std::wstring::npos);
}
