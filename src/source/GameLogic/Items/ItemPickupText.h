#pragma once

#include <cstdint>
#include <string>

struct tagITEM;

namespace GameLogic::Items
{
// An item's name as its tooltip title shows it: the ancient set name, the item name with its level
// ("Dragon Armor +9") and "Excellent" for excellent items; "Excellent Dragon Armor +9".
std::wstring ItemDisplayName(const tagITEM& item);
// "1,234,567": Zen with grouped digits.
std::wstring GroupedZen(std::uint64_t zen);
// The chat line of a pickup (packet 22): "3,186,000 Zen Obtained", "Excellent Dragon Armor +9 Obtained".
std::wstring ZenObtainedText(std::uint64_t zen);
std::wstring ItemObtainedText(const tagITEM& item);
} // namespace GameLogic::Items
