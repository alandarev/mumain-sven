#include "stdafx.h"
#include "UI/NPCs/NPCShop.h"
#include "UI/Core/WindowSystem.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/Placement/WindowPlacement.h"
#include "Engine/Object/ZzzInventory.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Core/Utilities/StringUtils.h"
#include "GameLogic/Social/GambleSystem.h"
#include "UI/Dialogs/GenericConfirmDialog.h"
#include "I18N/All.h"
#include "GameLogic/Items/ItemCategories.h"
#include "GameLogic/Items/ShopRestrictions.h"

#include <algorithm>
#include <numeric>
#include <charconv>
#include <optional>

extern int BuyCost;

namespace
{
constexpr int StockColumns = 8;
constexpr int StockRows = 15;
constexpr int MaximumQuantity = 100;
constexpr unsigned long long FeedbackDuration = 3500;
constexpr unsigned long long PurchaseSpacing = 200;

template <class View, class Value> void SyncValue(View& view, Value& field, const char* name, const Value& value)
{
    if (field == value)
        return;
    field = value;
    view.MarkDirty(name);
}

Rml::String ItemName(const ITEM* item)
{
    wchar_t name[256]{};
    GetItemName(item->Type, item->Level, name);
    return StringUtils::WideToNarrow(name);
}

Rml::String Amount(unsigned long long value)
{
    wchar_t text[256]{};
    ConvertGold(value, text);
    return StringUtils::WideToNarrow(text);
}

std::int64_t OfferEstimate(ITEM* item, int tax)
{
    const auto price = std::max<std::int64_t>(0, ItemValue(item, 0));
    return price + price * std::clamp(tax, 0, 100) / 100;
}

bool IsQuantityOffer(const ITEM* item)
{
    // Restrict batching to the basic recovery/antidote range whose durability is a stack count.
    return item && item->Type >= ITEM_POTION && item->Type <= ITEM_ANTIDOTE && !GambleSystem::Instance().IsGambleShop();
}

int OfferUnits(const ITEM* item)
{
    return IsQuantityOffer(item) ? std::max(1, static_cast<int>(item->Durability)) : 1;
}

std::optional<int> ParseOffers(const Rml::String& text)
{
    int count = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), count);
    if (error != std::errc{} || end != text.data() + text.size() || count < 1 || count > MaximumQuantity)
        return std::nullopt;
    return count;
}
} // namespace

using mu::ui::window::CInventoryCtrl;
using mu::ui::window::CNPCShop;

void CNPCShop::BindCounterModel(Rml::DataModelConstructor& c, NPCShopRmlModel& model)
{
    c.RegisterArray<std::vector<int>>();
    c.Bind("rows", &model.rows);
    c.Bind("context_name", &model.contextName);
    c.Bind("context_price", &model.contextPrice);
    c.Bind("context_hint", &model.contextHint);
    c.Bind("highlight_visible", &model.highlightVisible);
    c.Bind("highlight_column", &model.highlightColumn);
    c.Bind("highlight_row", &model.highlightRow);
    c.Bind("highlight_width", &model.highlightWidth);
    c.Bind("highlight_height", &model.highlightHeight);
    c.Bind("buy_pending", &model.buyPending);
    c.Bind("repair_active", &model.repairActive);
    c.Bind("quantity_open", &model.quantityOpen);
    c.Bind("quantity", &model.quantityText);
    c.Bind("quantity_valid", &model.quantityValid);
    c.Bind("quantity_name", &model.quantityName);
    c.Bind("quantity_total", &model.quantityTotal);
    c.BindEventCallback("quantity_less",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        {
                            auto& model = m_RmlView.GetModel();
                            model.quantityText =
                                std::to_string(std::max(1, ParseOffers(model.quantityText).value_or(1) - 1));
                            m_RmlView.MarkDirty("quantity");
                        });
    c.BindEventCallback("quantity_more",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        {
                            auto& model = m_RmlView.GetModel();
                            model.quantityText = std::to_string(
                                std::min(MaximumQuantity, ParseOffers(model.quantityText).value_or(1) + 1));
                            m_RmlView.MarkDirty("quantity");
                        });
    c.BindEventCallback("quantity_buy",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { ConfirmQuantity(); });
    c.BindEventCallback("quantity_key",
                        [this](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList&)
                        {
                            const int key = event.GetParameter<int>("key_identifier", 0);
                            if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER)
                                ConfirmQuantity();
                            else if (key == Rml::Input::KI_ESCAPE)
                            {
                                m_RmlView.GetModel().quantityOpen = false;
                                m_RmlView.MarkDirty("quantity_open");
                            }
                            else
                                return;
                            event.StopPropagation();
                        });
    c.BindEventCallback("quantity_cancel",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        {
                            m_RmlView.GetModel().quantityOpen = false;
                            m_RmlView.MarkDirty("quantity_open");
                            m_Purchases.Stop();
                        });
    c.BindEventCallback("counter_close",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        {
                            if (!IsTransactionPending())
                                g_pNewUISystem->Hide(mu::ui::window::INTERFACE_NPCSHOP);
                        });
}

void CNPCShop::SyncCounterGrid()
{
    const bool compact = UI::RmlBridge::ThemeUsesCompactNpcShop();
    int visibleRows = compact ? 1 : StockRows;
    if (compact)
        for (size_t i = 0; i < m_pNewInventoryCtrl->GetNumberOfItems(); ++i)
        {
            const ITEM* item = m_pNewInventoryCtrl->GetItem(static_cast<int>(i));
            visibleRows = std::max(visibleRows, item->y + ItemAttribute[item->Type].Height);
        }
    visibleRows = std::clamp(visibleRows, 1, StockRows);
    m_pNewInventoryCtrl->SetPresentation(this, compact);
    m_pNewInventoryCtrl->SetVisibleRowCount(visibleRows);
    std::vector<int> rows(visibleRows);
    std::iota(rows.begin(), rows.end(), 0);
    if (m_RmlView.GetModel().rows != rows || m_RmlView.GetModel().repairVisible != m_bRepairShop)
        UI::Placement::Invalidate();
    SyncValue(m_RmlView, m_RmlView.GetModel().rows, "rows", rows);
}

void CNPCShop::SyncCounterContext()
{
    auto& model = m_RmlView.GetModel();
    const bool pending = m_Purchases.Active();
    ITEM* item =
        pending ? m_pNewInventoryCtrl->FindItemByKey(m_Purchases.Key()) : m_pNewInventoryCtrl->FindItemAtPointer();
    Rml::String name = item ? ItemName(item) : "Hover an item";
    Rml::String price = item ? "Est. " + Amount(OfferEstimate(item, m_iTaxRate)) + " Zen" : "";
    if (item && m_iTaxRate > 0)
        price += " · incl. tax";
    Rml::String hint = IsQuantityOffer(item) ? "Click to buy · Shift-click for quantity" : "Click stock to buy";
    if (m_dwShopState == SHOP_STATE_REPAIR)
    {
        item = nullptr;
        name = "Repair mode";
        price = "Est. all: " + Amount(AllRepairGold) + " Zen";
        hint = "Click damaged gear · Esc to finish";
    }
    if (auto* picked = CInventoryCtrl::GetPickedItem(); picked && m_pNewInventoryCtrl->ContainsPointer())
    {
        item = nullptr;
        name = ItemName(picked->GetItem());
        price = "Est. +" + Amount(ItemValue(picked->GetItem())) + " Zen";
        hint = GameLogic::Items::IsSellingBan(picked->GetItem()) ? "This item cannot be sold" : "Release to sell";
    }
    if (m_bSellingItem)
        hint = "Selling…";
    else if (m_RepairPending)
        hint = "Repairing…";
    else if (pending)
        hint = "Buying… " + std::to_string(m_Purchases.Confirmed()) + " / " + std::to_string(m_Purchases.Requested());
    else if (GetTickCount64() < m_FeedbackUntil)
        hint = m_Feedback;
    SyncValue(m_RmlView, model.contextName, "context_name", name);
    SyncValue(m_RmlView, model.contextPrice, "context_price", price);
    SyncValue(m_RmlView, model.contextHint, "context_hint", hint);
    SyncValue(m_RmlView, model.buyPending, "buy_pending", pending);
    SyncValue(m_RmlView, model.repairActive, "repair_active", m_dwShopState == SHOP_STATE_REPAIR);
    SyncValue(m_RmlView, model.highlightVisible, "highlight_visible", item != nullptr);
    if (!item)
        return;
    SyncValue(m_RmlView, model.highlightColumn, "highlight_column", static_cast<int>(item->x));
    SyncValue(m_RmlView, model.highlightRow, "highlight_row", static_cast<int>(item->y));
    SyncValue(m_RmlView, model.highlightWidth, "highlight_width", static_cast<int>(ItemAttribute[item->Type].Width));
    SyncValue(m_RmlView, model.highlightHeight, "highlight_height", static_cast<int>(ItemAttribute[item->Type].Height));
}

void CNPCShop::SyncQuantityModel()
{
    auto& model = m_RmlView.GetModel();
    if (!model.quantityOpen)
        return;
    ITEM* item = m_pNewInventoryCtrl->FindItemByKey(m_QuantityKey);
    if (!IsQuantityOffer(item) || OfferEstimate(item, m_iTaxRate) != m_OfferEstimate || m_iTaxRate != m_BatchTax ||
        OfferUnits(item) != m_OfferUnits)
    {
        model.quantityOpen = false;
        m_RmlView.MarkDirty("quantity_open");
        SetFeedback("Offer changed. Choose it again.");
        return;
    }
    const auto quantity = ParseOffers(model.quantityText);
    SyncValue(m_RmlView, model.quantityValid, "quantity_valid", quantity.has_value());
    if (quantity)
        model.quantity = *quantity;
    const Rml::String total = quantity
                                  ? std::to_string(*quantity * OfferUnits(item)) + " items · Est. " +
                                        Amount(static_cast<unsigned long long>(m_OfferEstimate) * *quantity) + " Zen"
                                  : "Enter 1–100 offers";
    SyncValue(m_RmlView, model.quantityTotal, "quantity_total", total);
}

void CNPCShop::SyncCounterModel()
{
    SyncCounterGrid();
    if (!UI::RmlBridge::ThemeUsesCompactNpcShop())
    {
        m_RmlView.GetModel().quantityOpen = false;
        m_RmlView.MarkDirty("quantity_open");
        return;
    }
    auto& model = m_RmlView.GetModel();
    if (!m_MerchantName.empty())
        SyncValue(m_RmlView, model.title, "title", m_MerchantName);
    SyncCounterContext();
    SyncQuantityModel();
}

bool CNPCShop::ProcessCounterClick(ITEM* item)
{
    if (!UI::RmlBridge::ThemeUsesCompactNpcShop())
        return false;
    if (IsTransactionPending() || m_dwShopState == SHOP_STATE_REPAIR)
        return true;
    if (GambleSystem::Instance().IsGambleShop())
        return false;
    if (mu::ui::window::IsRepeat(VK_SHIFT) && IsQuantityOffer(item))
        OpenQuantity(item);
    else
        StartPurchase(item, 1);
    return true;
}

void CNPCShop::OpenQuantity(ITEM* item)
{
    m_QuantityKey = item->Key;
    m_OfferUnits = OfferUnits(item);
    m_OfferEstimate = OfferEstimate(item, m_iTaxRate);
    m_BatchTax = m_iTaxRate;
    auto& model = m_RmlView.GetModel();
    model.quantityOpen = true;
    model.quantity = 1;
    model.quantityText = "1";
    model.quantityName = ItemName(item) + " · " + std::to_string(OfferUnits(item)) + " per offer";
    m_RmlView.MarkDirty("quantity_open");
    m_RmlView.MarkDirty("quantity");
    m_RmlView.MarkDirty("quantity_name");
}

void CNPCShop::ConfirmQuantity()
{
    auto& model = m_RmlView.GetModel();
    if (!model.quantityOpen)
        return;
    SyncQuantityModel();
    if (!model.quantityOpen || !model.quantityValid)
        return;
    auto* item = m_pNewInventoryCtrl->FindItemByKey(m_QuantityKey);
    StartPurchase(item, model.quantity);
    model.quantityOpen = false;
    m_RmlView.MarkDirty("quantity_open");
}

void CNPCShop::StartPurchase(ITEM* item, int offers)
{
    if (!item || IsTransactionPending() || BuyCost != 0 || !IsVisible())
        return;
    if (!m_Purchases.Begin(item->y * StockColumns + item->x, item->Key, offers))
        return;
    m_OfferUnits = OfferUnits(item);
    m_OfferEstimate = OfferEstimate(item, m_iTaxRate);
    m_BatchTax = m_iTaxRate;
    m_NextPurchaseAt = 0;
    UpdatePurchase();
}

void CNPCShop::UpdatePurchase()
{
    if (!m_Purchases.Active() || m_Purchases.InFlight() || GetTickCount64() < m_NextPurchaseAt)
        return;
    auto* item = m_pNewInventoryCtrl->FindItemByKey(m_Purchases.Key());
    if (!IsVisible() || !UI::RmlBridge::ThemeUsesCompactNpcShop() || !item ||
        item->y * StockColumns + item->x != m_Purchases.Slot() || OfferEstimate(item, m_iTaxRate) != m_OfferEstimate ||
        OfferUnits(item) != m_OfferUnits || m_iTaxRate != m_BatchTax)
    {
        m_Purchases.Stop();
        SetFeedback("Stopped: offer changed · " + std::to_string(m_Purchases.Confirmed() * m_OfferUnits) + " bought");
        return;
    }
    if (BuyCost != 0 || !m_Purchases.Send())
        return;
    // The compact counter uses BuyCost only as the existing nonzero request guard.
    BuyCost = 1;
    SocketClient->ToGameServer()->SendBuyItemFromNpcRequest(m_Purchases.Slot());
}

bool CNPCShop::IsTransactionPending() const
{
    return m_bSellingItem || m_RepairPending || m_Purchases.Active();
}

void CNPCShop::PurchaseFinished(bool success)
{
    if (!m_Purchases.Answer(success))
        return;
    m_NextPurchaseAt = GetTickCount64() + PurchaseSpacing;
    const auto delivered = std::to_string(m_Purchases.Confirmed() * m_OfferUnits);
    SetFeedback(success ? "Bought " + delivered + (m_OfferUnits > 1 ? " items" : " offer(s)")
                        : "Purchase refused · " + delivered + " bought");
}

void CNPCShop::SaleFinished(bool success)
{
    if (!m_bSellingItem)
        return;
    m_bSellingItem = false;
    SetFeedback(success ? "Item sold" : "Sale refused · item restored");
}

void CNPCShop::RepairAll()
{
    if (!m_bRepairShop || IsTransactionPending() || CInventoryCtrl::GetPickedItem())
        return;
    if (AllRepairGold == 0)
    {
        SetFeedback("Everything is fully repaired");
        return;
    }
    m_RepairPending = UI::RmlBridge::ThemeUsesCompactNpcShop();
    m_RepairStartedAt = GetTickCount64();
    m_RepairGoldBefore = AllRepairGold;
    SocketClient->ToGameServer()->SendRepairItemRequest(0xFF, 0);
}

void CNPCShop::UpdateRepairFeedback()
{
    if (!m_RepairPending)
        return;
    // Some servers confirm repairs through item durability updates, without a 0x34 result.
    if (AllRepairGold < m_RepairGoldBefore)
        RepairFinished(true);
    else if (GetTickCount64() - m_RepairStartedAt >= 3000)
    {
        m_RepairPending = false;
        SetFeedback("No repair confirmed");
    }
}

void CNPCShop::RepairFinished(bool success)
{
    if (!IsVisible())
        return;
    m_RepairPending = false;
    SetFeedback(success ? "Repair complete" : "Repair refused");
}

void CNPCShop::SetFeedback(const Rml::String& text)
{
    m_Feedback = text;
    m_FeedbackUntil = GetTickCount64() + FeedbackDuration;
}

void CNPCShop::ResetCounter()
{
    m_Purchases = {};
    m_RepairPending = false;
    m_QuantityKey = 0;
    m_FeedbackUntil = 0;
    m_RmlView.GetModel().quantityOpen = false;
    m_RmlView.MarkDirty("quantity_open");
}

void CNPCShop::SetMerchant(int characterIndex)
{
    m_MerchantName.clear();
    SetRepairShop(false);
    if (!CharactersClient || characterIndex < 0 || characterIndex >= MAX_CHARACTERS_CLIENT ||
        !CharactersClient[characterIndex].Object.Live)
        return;
    const auto& merchant = CharactersClient[characterIndex];
    m_MerchantName = StringUtils::WideToNarrow(merchant.ID);
    const int monster = merchant.MonsterIndex;
    SetRepairShop(monster == MONSTER_EO_THE_CRAFTSMAN || monster == MONSTER_ZIENNA_THE_WEAPONS_MERCHANT ||
                  monster == MONSTER_HANZO_THE_BLACKSMITH || monster == MONSTER_RHEA ||
                  monster == MONSTER_WEAPONS_MERCHANT_BOLO);
}

bool CNPCShop::SubmitSale()
{
    auto* picked = CInventoryCtrl::GetPickedItem();
    if (!picked)
        return false;
    if (!IsVisible() || IsTransactionPending())
    {
        CInventoryCtrl::BackupPickedItem();
        return false;
    }
    auto* item = picked->GetItem();
    const int slot = picked->GetSourceLinealPos();
    if (!item || picked->GetSourceStorageType() != STORAGE_TYPE::INVENTORY || slot < MAX_EQUIPMENT_INDEX ||
        slot >= MAX_MY_INVENTORY_EX_INDEX || GameLogic::Items::IsSellingBan(item) ||
        item->Jewel_Of_Harmony_Option != 0 ||
        static_cast<unsigned long long>(CharacterMachine->Gold) + ItemValue(item) > 2000000000ULL)
    {
        CInventoryCtrl::BackupPickedItem();
        SetFeedback("Sale unavailable · item restored");
        return false;
    }
    m_bSellingItem = true;
    SocketClient->ToGameServer()->SendSellItemToNpcRequest(slot);
    return true;
}

bool CNPCShop::SellPickedItem()
{
    auto* picked = CInventoryCtrl::GetPickedItem();
    if (!picked || IsTransactionPending() || m_dwShopState == SHOP_STATE_REPAIR)
        return false;
    auto* item = picked->GetItem();
    if (!item)
        return false;
    if (GameLogic::Items::IsSellingBan(item) || item->Jewel_Of_Harmony_Option != 0 ||
        static_cast<unsigned long long>(CharacterMachine->Gold) + ItemValue(item) > 2000000000ULL)
        return SubmitSale();
    if (!GameLogic::Items::IsHighValueItem(item))
        return SubmitSale();

    mu::ui::window::GenericDialogConfig cfg;
    cfg.showCancel = true;
    cfg.item3D = *item;
    const std::wstring name = StringUtils::NarrowToWide(ItemName(item));
    const std::wstring proceeds = L"Est. +" + StringUtils::NarrowToWide(Amount(ItemValue(item))) + L" Zen";
    cfg.lines = {{name, true}, {proceeds, false}, {I18N::Game::AreYouSureYouWantToSellIt, true}};
    const auto key = item->Key;
    cfg.onPrimary = [this, key]
    {
        auto* current = CInventoryCtrl::GetPickedItem();
        if (current && current->GetItem() && current->GetItem()->Key == key)
            SubmitSale();
        else
            CInventoryCtrl::BackupPickedItem();
    };
    cfg.onCancel = [this]
    {
        CInventoryCtrl::BackupPickedItem();
        SetFeedback("Sale cancelled");
    };
    mu::ui::window::g_pGenericConfirmDialog->Show(std::move(cfg));
    picked->HidePickedItem();
    return true;
}
