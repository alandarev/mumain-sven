#pragma once
#include <cstdint>
#include "UI/AuctionHouse/WindowModel.h"
struct tagITEM;

namespace UI::AuctionHouse
{
RowModel DescribeItem(const tagITEM* item, int index);
bool SameInventoryItem(const tagITEM* item, const tagITEM* saved, std::uint32_t sourceKey);
} // namespace UI::AuctionHouse
