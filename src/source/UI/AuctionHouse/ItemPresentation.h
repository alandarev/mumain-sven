#pragma once
#include "UI/AuctionHouse/WindowModel.h"
struct tagITEM;

namespace UI::AuctionHouse
{
RowModel DescribeItem(const tagITEM* item, int index);
bool SameInventoryItem(const tagITEM* item, const tagITEM* saved);
} // namespace UI::AuctionHouse
