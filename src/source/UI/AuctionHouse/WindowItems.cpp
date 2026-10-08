#include "stdafx.h"
#include "UI/AuctionHouse/Window.h"
#include "UI/AuctionHouse/ItemPresentation.h"
#include "UI/Core/WindowSystem.h"
#include "UI/HUD/ItemHotKey.h"
#include "UI/RmlBridge/RmlTooltip.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Core/Utilities/StringUtils.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <cmath>
#include <limits>

namespace UI::AuctionHouse
{
namespace
{
// Every line of a multi-line text; an empty text is one empty line, as the tooltip always showed it.
std::vector<std::string> SplitLines(const std::string& text)
{
    std::vector<std::string> lines;
    std::size_t start = 0;
    for (auto end = text.find('\n');; end = text.find('\n', start))
    {
        lines.push_back(text.substr(start, end == std::string::npos ? end : end - start));
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return lines;
}
} // namespace

void Window::ItemDeleter::operator()(ITEM* item) const
{
    if (item && g_pNewItemMng)
        g_pNewItemMng->DeleteItem(item);
}

Window::Item Window::Decode(std::span<const std::uint8_t> bytes) const
{
    if (bytes.size() < 5 || !g_pNewItemMng)
        return Item{};
    const int type = (bytes[0] >> 4) * MAX_ITEM_INDEX + ((bytes[0] & 15) << 8) + bytes[1];
    if (type >= MAX_ITEM)
        return Item{};
    return Item(g_pNewItemMng->CreateItem(bytes));
}

Window::Item Window::GoodsItem(const Market::GoodsRow& goods) const
{
    if (goods.type >= MAX_ITEM || !g_pNewItemMng)
        return Item{};
    ItemCreationParams params{};
    params.Group = goods.type / MAX_ITEM_INDEX;
    params.Number = goods.type % MAX_ITEM_INDEX;
    params.Level = goods.level;
    params.Durability = 0;
    return Item(g_pNewItemMng->CreateItemByParameters(&params));
}

ITEM* Window::InventoryItem(int slot) const
{
    if (slot < MAX_EQUIPMENT_INDEX || slot > std::numeric_limits<std::uint8_t>::max())
        return nullptr;
    ITEM* item = g_pMyInventory ? g_pMyInventory->FindItem(slot) : nullptr;
    return item ? item : g_pMyInventoryExt ? g_pMyInventoryExt->FindItem(slot) : nullptr;
}

RowModel Window::Describe(ITEM* item, int index) const
{
    auto row = DescribeItem(item, index);
    MarkSearchedOptions(row.excellent, m_Filter.excellentMask);
    return row;
}

void Window::ClearItems()
{
    m_SelectedIcon = {};
    m_SellItem.reset();
    m_Icons.clear();
    m_InventoryIcons.clear();
}

void Window::RefreshInventory()
{
    auto& model = m_View.GetModel();
    std::vector<RowModel> rows;
    // The tile of the item shown in the details is marked; SwitchTab clears the details.
    const int markedSlot = model.selected ? m_SellSlot : -1;
    // Start slots only: spanning cells resolve to the same item and must not be duplicated.
    for (int slot = MAX_EQUIPMENT_INDEX; slot <= std::numeric_limits<std::uint8_t>::max(); ++slot)
    {
        auto* item = InventoryItem(slot);
        if (!item)
            continue;
        auto* control = g_pMyInventory->GetInventoryCtrl();
        if (!control || control->FindItem(slot) != item)
            control = g_pMyInventoryExt->TryGetExtensionByInventoryIndex(slot);
        if (!control || control->GetIndexByItem(item) != slot)
            continue;
        auto row = Describe(item, slot);
        row.selected = slot == markedSlot;
        // Group identical jewel presentations only. The first real slot remains the
        // selection anchor; CheckSell still decides eligibility and sellable quantity.
        const bool groupable = item->Type / MAX_ITEM_INDEX >= 12 && MatchesGoodsName(row.name, Category::Jewels, {});
        auto existing = groupable ? std::find_if(rows.begin(), rows.end(),
                                                 [this, item, &row](const auto& old)
                                                 {
                                                     const auto* anchor = InventoryItem(old.index);
                                                     return anchor && anchor->Type == item->Type &&
                                                            anchor->Level == item->Level && old.detail == row.detail;
                                                 })
                                  : rows.end();
        if (existing == rows.end())
            rows.push_back(std::move(row));
        else
            ++existing->units;
    }
    const bool changed = rows.size() != model.inventory.size() ||
                         !std::equal(rows.begin(), rows.end(), model.inventory.begin(),
                                     [](const auto& a, const auto& b)
                                     {
                                         return a.index == b.index && a.name == b.name && a.detail == b.detail &&
                                                a.units == b.units && a.selected == b.selected;
                                     });
    if (!changed)
        return;
    model.inventory = std::move(rows);
    m_InventoryIcons.clear();
    for (const auto& row : model.inventory)
    {
        auto* item = InventoryItem(row.index);
        m_InventoryIcons.push_back({Item(item ? g_pNewItemMng->DuplicateItem(item) : nullptr), nullptr});
    }
    m_Dirty = true;
}

bool Window::SellItemUnchanged() const
{
    return SameInventoryItem(InventoryItem(m_SellSlot), m_SellItem.get(), m_SellKey);
}

void Window::SelectInventory(int slot)
{
    if (m_View.GetModel().busy)
        return;
    auto* item = InventoryItem(slot);
    if (!item)
    {
        SetStatus("Item no longer available.");
        return;
    }
    ClearFeedback();
    m_SellSlot = slot;
    m_SellKey = item->Key;
    auto& model = m_View.GetModel();
    const auto row = Describe(item, slot);
    model.selectedSeller.clear();
    model.selected = true;
    model.selectedName = row.name;
    model.selectedDetail = row.stats;
    model.selectedOptions = row.options;
    model.selectedRequirements = row.requirements;
    model.selectedEquipReasons.clear();
    model.selectedTone = row.tone;
    m_SelectedIcon = {Item(g_pNewItemMng->DuplicateItem(item)), nullptr};
    m_SellItem = Item(g_pNewItemMng->DuplicateItem(item));
    m_SellCheck.reset();
    m_SellQuote.reset();
    m_SellIdentity.reset();
    m_PriceEntry.Reset();
    model.quantity = "1";
    model.maxQuantity.clear();
    model.marketLowest.clear();
    model.sellTiers.clear();
    if (const auto draft = m_SellDrafts.find(m_SellKey); draft != m_SellDrafts.end())
    {
        model.quantity = draft->second.quantity;
        m_PriceEntry.Prefill(Market::GetState().GetRules(), draft->second.price, std::nullopt, 0);
    }
    ShowPrice();
    RefreshPriceEntry();
    model.sellReady = false;
    model.sellGoods = false;
    // The tile stays marked while the item is priced, also after Back to selling.
    MarkSelectedInventory(model.inventory, m_SellSlot);
    DismissReview();
    QueueRead(Read::Sell);
    m_Dirty = true;
}

void Window::SelectRow(int index)
{
    auto& model = m_View.GetModel();
    if (index < 0 || static_cast<std::size_t>(index) >= model.rows.size() || model.busy)
        return;
    ClearFeedback();
    m_ReviewGate.Cancel();
    m_Selected = index;
    for (auto& row : model.rows)
        row.selected = row.index == index;
    const auto& row = model.rows[index];
    model.selectedSeller = model.tab == "browse" && !model.goods && static_cast<std::size_t>(index) < m_Listings.size()
                               ? m_Listings[index].seller
                               : "";
    model.selected = true;
    model.selectedName = row.name;
    model.selectedDetail = row.stats;
    model.selectedOptions = row.options;
    model.selectedRequirements = row.requirements;
    model.selectedPrice = row.price;
    model.selectedTone = row.tone;
    model.cannotEquip = row.cannotEquip;
    model.selectedEquipReasons = row.equipReasons;
    m_SelectedIcon = {};
    if (static_cast<std::size_t>(index) < m_Icons.size() && m_Icons[index].item)
        m_SelectedIcon.item.reset(g_pNewItemMng->DuplicateItem(m_Icons[index].item.get()));
    model.buyQuantity = "1";
    ClearQuote();
    if (model.tab == "browse" && model.goods)
        QueueRead(Read::Quote);
    m_Dirty = true;
}

void Window::SyncIcons()
{
    auto* doc = m_View.Document();
    if (!doc)
        return;
    auto sync = [this](Icon& icon, Rml::Element* image)
    {
        if (image)
            image->SetClass("loaded", icon.item && icon.target && !icon.target->Source().empty());
        if (!image || !icon.item)
        {
            if (icon.target)
                icon.target->SetEnabled(false);
            return;
        }
        const auto bounds = image->GetAbsoluteOffset(Rml::BoxArea::Border);
        const auto size = image->GetBox().GetSize(Rml::BoxArea::Content);
        auto* pane = image->GetParentNode();
        while (pane && !pane->IsClassSet("scroll-pane"))
            pane = pane->GetParentNode();
        // Both offsets share the panel transform, so centering and dragging cancel out.
        const float top = pane ? bounds.y - pane->GetAbsoluteOffset(Rml::BoxArea::Content).y : 0.f;
        const bool shown =
            IsVisible() && image->IsVisible(true) && (!pane || (top + size.y > 0 && top < pane->GetClientHeight()));
        if (!shown)
        {
            if (icon.target)
                icon.target->SetEnabled(false);
            return;
        }
        if (!icon.target)
        {
            auto* item = icon.item.get();
            icon.target = std::make_unique<UI::RmlBridge::RenderTarget>(
                [item](auto width, auto height)
                { mu::ui::window::CItemHotKey::RenderItemModel(item, width, height, false, true); });
        }
        icon.target->SetEnabled(true);
        icon.target->Resize(std::max(1l, std::lround(size.x)), std::max(1l, std::lround(size.y)));
        image->SetClass("loaded", !icon.target->Source().empty());
        if (image->GetAttribute<Rml::String>("src", {}) != icon.target->Source())
            image->SetAttribute("src", icon.target->Source());
    };
    for (std::size_t i = 0; i < m_Icons.size(); ++i)
        sync(m_Icons[i], doc->GetElementById("row-icon-" + std::to_string(i)));
    for (std::size_t i = 0; i < m_InventoryIcons.size(); ++i)
        sync(m_InventoryIcons[i], doc->GetElementById("inventory-icon-" + std::to_string(i)));
    sync(m_SelectedIcon, doc->GetElementById("selected-icon"));
}

void Window::Hover(int index, bool inventory)
{
    const auto& rows = inventory ? m_View.GetModel().inventory : m_View.GetModel().rows;
    if (index < 0 || static_cast<std::size_t>(index) >= rows.size())
        return;
    UI::RmlBridge::Tooltip::Config config;
    config.anchorX = static_cast<float>(MouseX);
    config.anchorY = static_cast<float>(MouseY);
    const auto& row = rows[index];
    config.lines.push_back({row.name, UI::RmlBridge::Tooltip::LineColor::Yellow, true});
    for (const auto& line : SplitLines(row.detail))
        config.lines.push_back({line});
    if (!row.exactPrice.empty())
        config.lines.push_back({row.exactPrice, UI::RmlBridge::Tooltip::LineColor::Yellow});
    if (row.cannotEquip)
    {
        config.lines.push_back({"Cannot Equip", UI::RmlBridge::Tooltip::LineColor::Red});
        if (!row.equipReasons.empty())
            for (const auto& reason : SplitLines(row.equipReasons))
                config.lines.push_back({reason, UI::RmlBridge::Tooltip::LineColor::Red});
    }
    UI::RmlBridge::Tooltip::Show(config, this);
}
} // namespace UI::AuctionHouse
