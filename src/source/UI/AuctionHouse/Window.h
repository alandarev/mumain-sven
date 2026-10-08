#pragma once

#include "stdafx.h"

#include "GameLogic/AuctionHouse/AuctionHouse.h"
#include "UI/AuctionHouse/Presentation.h"
#include "UI/AuctionHouse/WindowModel.h"
#include "UI/Core/WindowObject.h"
#include "UI/RmlBridge/RmlRenderTarget.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <deque>
#include <functional>
#include <memory>

namespace mu::ui::window
{
class CManager;
}

namespace UI::AuctionHouse
{
class Window : public mu::ui::window::CObject, public Market::IListener
{
public:
    Window();
    ~Window() override;
    bool Create(mu::ui::window::CManager* manager);
    void Release();
    void Show(bool show) override;
    bool Update() override;
    bool Render() override
    {
        return true;
    }
    bool UpdateMouseEvent() override;
    bool UpdateKeyEvent() override;
    float GetLayerDepth() override
    {
        return 10.64f;
    }
    float GetKeyEventOrder() override
    {
        return 10.5f;
    }
    void OnOpened() override;
    void OnClosed() override;
    void OnChanged(Market::Operation operation) override;
    void OnNotice(const Market::Notice& notice) override;

private:
    struct ItemDeleter
    {
        void operator()(ITEM* item) const;
    };
    using Item = std::unique_ptr<ITEM, ItemDeleter>;
    struct Icon
    {
        Item item;
        std::unique_ptr<UI::RmlBridge::RenderTarget> target;
    };
    enum class Read : std::uint8_t
    {
        None,
        Search,
        Goods,
        Own,
        Collection,
        Quote,
        Sell
    };
    enum class Action : std::uint8_t
    {
        None,
        BuyItem,
        BuyGoods,
        Publish,
        Cancel,
        Claim
    };
    struct Review
    {
        Action action = Action::None;
        Market::EntityId id{};
        Market::Zen amount = 0, fee = 0;
        std::uint32_t quantity = 0, itemKey = 0, revision = 0;
        std::uint16_t type = 0;
        std::uint8_t level = 0, slot = 0, duration = 0;
    };

    void Bind(Rml::DataModelConstructor& constructor, WindowModel& model);
    void BindEvents(Rml::DataModelConstructor& constructor);
    void BindFilterEvents(Rml::DataModelConstructor& constructor);
    void Built();
    void Dirty();
    void SetStatus(const std::string& text, bool error = true);
    void SwitchTab(const Rml::String& tab);
    void FilterChanged();
    void OpenFilters();
    void ApplyFilters();
    void DismissFilters();
    void ClearFilter(const Rml::String& key);
    void RefreshFilterSummary();
    bool CompileFilter();
    void BuildTypeChoices(int equipment);
    bool ApplyTypeChoice();
    bool CompileGoodsTypes(const std::string& search, int category);
    void QueueRead(Read read, bool restart = false);
    void PumpRequests(RequestClock::Time now);
    bool SendRead(Read read);
    void HandlePage(Market::Operation operation);
    void HandleOutcome(Market::Operation operation);
    void RefreshRows();
    void RefreshInventory();
    void RefreshRules();
    void RefreshSelling();
    void RefreshQuote();
    void SelectRow(int index);
    void SelectInventory(int slot);
    void ReviewBuy();
    void ReviewSale();
    void ReviewCancel(int index);
    void ReviewClaim(int index);
    void OpenReview(Review review, const Rml::String& name, const Rml::String& title, const Rml::String& action);
    void DismissReview();
    void Confirm();
    bool SendAction();
    void StepPrice(bool up);
    void SyncIcons();
    void Hover(int index, bool inventory);
    void ClearItems();
    void CheckScroll();
    bool SellItemUnchanged() const;
    ITEM* InventoryItem(int slot) const;
    RowModel Describe(ITEM* item, int index) const;
    Item Decode(std::span<const std::uint8_t> bytes) const;
    Item GoodsItem(const Market::GoodsRow& goods) const;

    mu::ui::window::CManager* m_Manager = nullptr;
    UI::RmlBridge::ThemedView<WindowModel> m_View;
    RequestClock m_Clock;
    RequestClock::Time m_SearchAt{}, m_ReadSentAt{}, m_StatusUntil{}, m_LastInventory{}, m_LastExpiry{},
        m_ActionSentAt{};
    std::deque<std::function<void()>> m_Events;
    Read m_Read = Read::None, m_SentRead = Read::None;
    bool m_ReviewNeedsFocus = false;
    bool m_Restart = true, m_SentRestart = true, m_Dirty = true, m_ActionQueued = false;
    std::uint32_t m_Generation = 0, m_SentGeneration = 0;
    std::uint16_t m_Page = 0;
    std::uint32_t m_Total = 0, m_Loaded = 0;
    Market::Zen m_Wallet = 0;
    std::vector<Market::ReceivedTime> m_ListingTimes;
    int m_Selected = -1, m_SellSlot = -1;
    std::uint32_t m_SellKey = 0;
    Market::SearchFilter m_Filter;
    std::vector<std::uint16_t> m_GoodsTypes;
    std::vector<Market::ListingRow> m_Listings;
    std::vector<Market::GoodsRow> m_Goods;
    std::vector<Market::CollectionEntry> m_Collection;
    std::vector<Icon> m_Icons, m_InventoryIcons;
    Icon m_SelectedIcon;
    Review m_Review;
};
} // namespace UI::AuctionHouse
