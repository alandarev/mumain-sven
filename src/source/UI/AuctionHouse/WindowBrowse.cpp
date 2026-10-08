#include "stdafx.h"
#include "UI/AuctionHouse/Window.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Core/Utilities/StringUtils.h"
#include <limits>

namespace UI::AuctionHouse
{
namespace
{
Category CategoryOf(const Rml::String& value)
{
    return value == "1" ? Category::Jewels : value == "2" ? Category::Materials : Category::Equipment;
}

Rml::String CategoryValue(Category category)
{
    return std::to_string(static_cast<int>(category));
}
} // namespace

BrowseQuery Window::CaptureQuery() const
{
    const auto& model = m_View.GetModel();
    BrowseQuery query;
    query.search = model.search;
    query.equipment = model.equipment;
    query.itemType = model.itemType;
    query.option = model.option;
    query.sort = model.sort;
    query.usable = model.usable;
    query.filters = AppliedFilters();
    return query;
}

void Window::ApplyQuery(const BrowseQuery& query)
{
    auto& model = m_View.GetModel();
    model.search = query.search;
    model.equipment = query.equipment;
    model.itemType = query.itemType;
    model.option = query.option;
    model.sort = query.sort;
    model.usable = query.usable;
    SetAppliedFilters(query.filters);
    m_Dirty = true;
}

void Window::SwitchCategory(const Rml::String& value)
{
    auto& model = m_View.GetModel();
    const auto category = CategoryOf(value);
    if (model.busy || model.review)
        return;
    ApplyQuery(m_Browse.SwitchTo(category, CaptureQuery()));
    model.category = CategoryValue(category);
    FilterChanged();
}

void Window::RefreshBrowseState()
{
    auto& model = m_View.GetModel();
    const auto category = CategoryOf(model.category);
    const auto query = CaptureQuery();
    model.emptyText = EmptyResultText(category, query);
    model.canClearName = !query.search.empty();
    model.canResetFilters = category == Category::Equipment && HasConstraints(query);
    model.comparing = m_Browse.ComparisonShown() ? m_Browse.Comparison()->subject : "";
    model.usableLabel = "Can equip";
    if (Hero && Hero->ID[0])
        model.usableLabel = StringUtils::WideToNarrow(Hero->ID) + " can equip";
    m_Dirty = true;
}

// Compare market (D9): the sell draft stays in the draft cache, Browse shows the same base item and
// upgrade level with every other filter cleared, and the player's own Browse queries wait for its end.
void Window::CompareMarket()
{
    auto& model = m_View.GetModel();
    auto* item = InventoryItem(m_SellSlot);
    if (!item || !ItemAttribute || !model.canCompare || model.busy || model.review)
        return;
    SaveSellDraft();
    MarketComparison comparison;
    comparison.subject = model.selectedName;
    comparison.type = static_cast<std::uint16_t>(item->Type);
    comparison.level = static_cast<std::uint8_t>(item->Level);
    comparison.baseName = StringUtils::WideToNarrow(ItemAttribute[item->Type].Name);
    comparison.sellSlot = m_SellSlot;
    comparison.sellKey = m_SellKey;
    ApplyQuery(m_Browse.BeginComparison(CaptureQuery(), std::move(comparison)));
    model.category = CategoryValue(Category::Equipment);
    SwitchTab("browse");
}

void Window::BackToSelling()
{
    auto& model = m_View.GetModel();
    if (!m_Browse.Comparison() || model.busy || model.review)
        return;
    const auto comparison = *m_Browse.Comparison();
    SwitchTab("sell");
    if (model.tab != "sell")
        return;
    // The item may have moved inside the inventory; its key still names it, and its lowest cell is its slot.
    const auto* kept = InventoryItem(comparison.sellSlot);
    int slot = kept && kept->Key == comparison.sellKey ? comparison.sellSlot : -1;
    for (int candidate = MAX_EQUIPMENT_INDEX; candidate <= std::numeric_limits<std::uint8_t>::max() && slot < 0;
         ++candidate)
        if (const auto* item = InventoryItem(candidate); item && item->Key == comparison.sellKey)
            slot = candidate;
    if (slot < 0)
    {
        SetStatus("The compared item is no longer in your inventory.");
        return;
    }
    SelectInventory(slot);
}

void Window::EndComparison()
{
    if (!m_Browse.Comparison())
        return;
    ApplyQuery(m_Browse.EndComparison());
    m_View.GetModel().category = CategoryValue(m_Browse.Active());
    RefreshFilterSummary();
    RefreshBrowseState();
}
} // namespace UI::AuctionHouse
