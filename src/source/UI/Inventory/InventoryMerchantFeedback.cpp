#include "stdafx.h"
#include "UI/Inventory/MyInventory.h"
#include "UI/Inventory/InventoryContents.h"
#include "UI/Core/WindowSystem.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "Core/Utilities/StringUtils.h"

using mu::ui::window::CMyInventory;

void CMyInventory::BindMerchantFeedback(Rml::DataModelConstructor& c, MyInventoryRmlModel& model)
{
    c.Bind("gold_label", &model.goldLabel);
    c.Bind("gold_gained", &model.goldGained);
    c.Bind("delivery_visible", &model.deliveryVisible);
    c.Bind("delivery_column", &model.deliveryColumn);
    c.Bind("delivery_row", &model.deliveryRow);
    c.Bind("delivery_width", &model.deliveryWidth);
    c.Bind("delivery_height", &model.deliveryHeight);
}

void CMyInventory::HighlightMerchantDelivery(int slot)
{
    if (!UI::RmlBridge::ThemeUsesEmbeddedInventory())
        return;
    m_MerchantDeliverySlot = slot;
    m_MerchantDeliveryUntil = GetTickCount64() + 1400;
}

void CMyInventory::SyncMerchantFeedback()
{
    auto& model = m_RmlView.GetModel();
    const auto now = GetTickCount64();
    const auto gold = static_cast<unsigned long long>(CharacterMachine->Gold);
    if (m_GoldObserved && gold != m_PreviousGold && g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_NPCSHOP))
    {
        wchar_t amount[256]{};
        ConvertGold(gold > m_PreviousGold ? gold - m_PreviousGold : m_PreviousGold - gold, amount);
        model.goldLabel = Rml::String(gold > m_PreviousGold ? "+" : "−") + StringUtils::WideToNarrow(amount) + " Zen";
        model.goldGained = gold > m_PreviousGold;
        m_GoldDeltaUntil = now + 2500;
        m_RmlView.MarkDirty("gold_label");
        m_RmlView.MarkDirty("gold_gained");
    }
    m_PreviousGold = gold;
    m_GoldObserved = true;
    if (now >= m_GoldDeltaUntil && model.goldLabel != "Zen")
    {
        model.goldLabel = "Zen";
        m_RmlView.MarkDirty("gold_label");
    }
    ITEM* item = UI::Inventory::FindPlayerItem(m_MerchantDeliverySlot);
    const int index = m_MerchantDeliverySlot - MAX_EQUIPMENT_INDEX;
    const bool visible = now < m_MerchantDeliveryUntil && item && index >= 0 &&
                         index / COLUMN_INVENTORY < ROW_INVENTORY + 4 * model.ownedExtensions;
    if (model.deliveryVisible != visible)
    {
        model.deliveryVisible = visible;
        m_RmlView.MarkDirty("delivery_visible");
    }
    if (!visible)
        return;
    const auto sync = [this](int& field, const char* name, int value)
    {
        if (field != value)
        {
            field = value;
            m_RmlView.MarkDirty(name);
        }
    };
    sync(model.deliveryColumn, "delivery_column", index % COLUMN_INVENTORY);
    sync(model.deliveryRow, "delivery_row", index / COLUMN_INVENTORY);
    sync(model.deliveryWidth, "delivery_width", ItemAttribute[item->Type].Width);
    sync(model.deliveryHeight, "delivery_height", ItemAttribute[item->Type].Height);
}
