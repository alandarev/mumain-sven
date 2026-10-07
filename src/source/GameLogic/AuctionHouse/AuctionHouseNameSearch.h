#pragma once

#include "GameLogic/AuctionHouse/AuctionHouse.h"

namespace GameLogic::AuctionHouse
{
struct ItemName
{
    std::uint16_t type = 0, slotMask = 0;
    std::wstring_view name;
};

NameMatch ResolveNames(std::wstring_view text, std::uint16_t slotMask, std::uint16_t groupMask,
                       std::span<const ItemName> catalogue, std::vector<std::uint16_t>& out);
} // namespace GameLogic::AuctionHouse
