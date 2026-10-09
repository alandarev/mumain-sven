#include "stdafx.h"
#include "GameLogic/Items/ItemPickupText.h"

#include "I18N/All.h"
#include "Engine/Object/ZzzInventory.h"
#include "GameLogic/Items/CSItemOption.h"

namespace GameLogic::Items
{
std::wstring ItemDisplayName(const tagITEM& item)
{
    wchar_t name[256]{};
    GetItemName(item.Type, item.Level, name);
    // The tooltip title (RenderItemInfo): set name, item name and level, "Excellent" in front; the
    // Horn of Fenrir uses its excellent bits for its kind, not as excellent options.
    wchar_t set[64]{};
    std::wstring text;
    if (item.AncientDiscriminator && g_csItemOption.GetSetItemName(set, item.Type, item.AncientDiscriminator))
        text = set;
    text += name;
    if (item.ExcellentFlags > 0 && item.Type != ITEM_HORN_OF_FENRIR)
        text = std::wstring(I18N::Game::Excellent) + L" " + text;
    return text;
}

std::wstring GroupedZen(std::uint64_t zen)
{
    auto digits = std::to_wstring(zen);
    for (auto at = static_cast<std::ptrdiff_t>(digits.size()) - 3; at > 0; at -= 3)
        digits.insert(static_cast<std::size_t>(at), L",");
    return digits;
}

std::wstring ZenObtainedText(std::uint64_t zen)
{
    return GroupedZen(zen) + L" " + I18N::Game::Zen + L" " + I18N::Game::Obtained;
}

std::wstring ItemObtainedText(const tagITEM& item)
{
    return ItemDisplayName(item) + L" " + I18N::Game::Obtained;
}
} // namespace GameLogic::Items
