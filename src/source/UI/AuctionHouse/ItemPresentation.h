#pragma once
#include <cstdint>
#include "GameLogic/AuctionHouse/AuctionHousePriceMemory.h"
#include "UI/AuctionHouse/WindowModel.h"
struct tagITEM;

namespace UI::AuctionHouse
{
RowModel DescribeItem(const tagITEM* item, int index);
bool SameInventoryItem(const tagITEM* item, const tagITEM* saved, std::uint32_t sourceKey);
// An equipment item's pricing identity: its type, level and options, never its serial, slot or durability.
GameLogic::AuctionHouse::PricingIdentity EquipmentPricingIdentity(const tagITEM& item);
} // namespace UI::AuctionHouse
