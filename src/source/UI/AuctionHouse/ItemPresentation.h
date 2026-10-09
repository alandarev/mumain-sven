#pragma once
#include <cstdint>
#include <optional>
#include "GameLogic/AuctionHouse/AuctionHousePriceMemory.h"
#include "UI/AuctionHouse/EquipmentDetails.h"
#include "UI/AuctionHouse/WindowModel.h"
#include <span>
#include <string>
struct tagITEM;

namespace mu::ui::window
{
class CItemMng;
}

namespace UI::AuctionHouse
{
// The name of an item serialized as in Collection rows and sale notices ("Excellent Dragon Armor +9");
// empty when the bytes do not decode to an item.
std::string SerializedItemName(std::span<const std::uint8_t> itemData, mu::ui::window::CItemMng& items);
RowModel DescribeItem(const tagITEM* item, int index);
// The current character's class and stats against the item's requirements, with the requirement reductions
// that the native item tooltip applies (harmony and socket options). Empty without a character.
std::optional<EquipFacts> CharacterEquipFacts(const tagITEM& item);
// Fills a row the server marked as not equippable with the reasons the character's facts show:
// the first one for the row (with the count of the others) and all of them for the details.
void ExplainCannotEquip(RowModel& row, const tagITEM& item);
bool SameInventoryItem(const tagITEM* item, const tagITEM* saved, std::uint32_t sourceKey);
// An equipment item's pricing identity: its type, level and options, never its serial, slot or durability.
GameLogic::AuctionHouse::PricingIdentity EquipmentPricingIdentity(const tagITEM& item);
} // namespace UI::AuctionHouse
