#include "stdafx.h"
#include "GameLogic/AuctionHouse/AuctionHouseNameSearch.h"
#include "Engine/Object/ZzzInfomation.h"
#include "GameLogic/Items/CSItemOption.h"

namespace GameLogic::AuctionHouse
{
namespace
{
std::uint16_t SlotMask(const ITEM_ATTRIBUTE& item, int type)
{
    const auto slot = item.m_byItemSlot;
    if (type / MAX_ITEM_INDEX == ITEM_GROUP_SHIELD)
        return 1u << 1;
    if (slot == EQUIPMENT_WEAPON_RIGHT || slot == EQUIPMENT_WEAPON_LEFT)
        return 1u << 0;
    if (slot >= EQUIPMENT_HELM && slot <= EQUIPMENT_WING)
        return static_cast<std::uint16_t>(1u << slot);
    if (slot == EQUIPMENT_AMULET)
        return 1u << 8;
    if (slot == EQUIPMENT_RING_RIGHT || slot == EQUIPMENT_RING_LEFT)
        return 1u << 9;
    if (slot == EQUIPMENT_HELPER)
        return 1u << 10;
    return 0;
}
} // namespace

NameMatch ResolveItemTypes(std::wstring_view text, std::uint16_t slotMask, std::uint16_t groupMask,
                           std::vector<std::uint16_t>& out)
{
    if (!ItemAttribute)
    {
        out.clear();
        return NameMatch::NoMatch;
    }
    std::vector<ItemName> names;
    names.reserve(MAX_ITEM);
    for (int type = 0; type < MAX_ITEM; ++type)
    {
        const auto& item = ItemAttribute[type];
        const auto end = std::find(std::begin(item.Name), std::end(item.Name), wchar_t{0});
        names.push_back({static_cast<std::uint16_t>(type), SlotMask(item, type),
                         std::wstring_view(item.Name, static_cast<std::size_t>(end - std::begin(item.Name)))});
    }
    return ResolveNames(text, slotMask, groupMask, names, out);
}
std::vector<std::wstring> AncientSets()
{
    std::vector<std::wstring> sets;
    if (!ItemAttribute)
        return sets;
    for (int type = 0; type < MAX_ITEM; ++type)
        for (int discriminator : {1, 2})
        {
            wchar_t name[MAX_ITEM_SET_NAME + 2]{};
            if (!g_csItemOption.GetSetItemName(name, type, discriminator))
                continue;
            std::wstring text(name);
            while (!text.empty() && text.back() == L' ')
                text.pop_back();
            if (std::find(sets.begin(), sets.end(), text) == sets.end())
                sets.push_back(std::move(text));
        }
    std::sort(sets.begin(), sets.end());
    return sets;
}

NameMatch ResolveAncientSet(std::wstring_view name, std::uint16_t slots, std::uint16_t groups,
                            std::vector<SearchEntry>& out)
{
    out.clear();
    if (!ItemAttribute)
        return NameMatch::NoMatch;
    for (int type = 0; type < MAX_ITEM; ++type)
    {
        if ((slots && !(SlotMask(ItemAttribute[type], type) & slots)) ||
            (groups && !(groups & (1u << (type / MAX_ITEM_INDEX)))))
            continue;
        for (int discriminator : {1, 2})
        {
            wchar_t set[MAX_ITEM_SET_NAME + 2]{};
            if (!g_csItemOption.GetSetItemName(set, type, discriminator))
                continue;
            std::wstring text(set);
            while (!text.empty() && text.back() == L' ')
                text.pop_back();
            if (text != name)
                continue;
            out.push_back({static_cast<std::uint16_t>(type), static_cast<std::uint8_t>(discriminator)});
            if (out.size() > 16)
            {
                out.clear();
                return NameMatch::TooBroad;
            }
        }
    }
    return out.empty() ? NameMatch::NoMatch : NameMatch::Ok;
}
} // namespace GameLogic::AuctionHouse
