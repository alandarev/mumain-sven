#pragma once

#include "stdafx.h"

#include "GameLogic/AuctionHouse/AuctionHouse.h"
#include "GameLogic/AuctionHouse/AuctionHousePriceMemory.h"
#include "UI/AuctionHouse/BrowseState.h"
#include "UI/AuctionHouse/GoodsQuote.h"
#include "UI/AuctionHouse/PriceEntry.h"
#include "UI/AuctionHouse/Receipts.h"
#include "UI/AuctionHouse/ReviewGate.h"
#include "UI/AuctionHouse/Presentation.h"
#include "UI/AuctionHouse/WindowModel.h"
#include "UI/Core/WindowObject.h"
#include "UI/RmlBridge/RmlRenderTarget.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <unordered_map>

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
        // The goods-list row of the selected goods, for its exact count after a capped quote (CR-04).
        GoodsCount,
        Sell,
        SellQuote
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
        // The reviewed item's name, for the receipt.
        std::string subject;
    };
    struct SellDraft
    {
        Rml::String price, quantity;
    };
    struct RememberedListing
    {
        Market::PricingIdentity identity;
        Market::Zen price = 0;
    };

    void Bind(Rml::DataModelConstructor& constructor, WindowModel& model);
    void BindEvents(Rml::DataModelConstructor& constructor);
    void BindFilterEvents(Rml::DataModelConstructor& constructor);
    void BindPriceEvents(Rml::DataModelConstructor& constructor);
    void Built();
    void Dirty();
    void SetStatus(const std::string& text, bool error = true);
    void ShowReceipt(const Receipt& receipt);
    void ShowStatus();
    void ShowNotices();
    // The player's next deliberate action clears the result line and the notices (D18, D19).
    void ClearFeedback();
    void RefreshListsAfterNotice();
    std::string NoticeItemName(const Market::Notice& notice) const;
    void SwitchTab(const Rml::String& tab);
    void FilterChanged();
    void OpenFilters();
    void ApplyFilters();
    void DismissFilters();
    void ClearFilter(const Rml::String& key);
    void RefreshFilterSummary();
    AdvancedFilters AppliedFilters() const;
    void SetAppliedFilters(const AdvancedFilters& filters);
    void RefreshSetChoices(const Rml::String& classLine);
    bool CheckDraftFilters();
    std::string ClearUnavailableFilters();
    BrowseQuery CaptureQuery() const;
    void ApplyQuery(const BrowseQuery& query);
    void SwitchCategory(const Rml::String& value);
    void RefreshBrowseState();
    void CompareMarket();
    void BackToSelling();
    void EndComparison();
    bool CompileFilter();
    bool CompileEquipmentFilter(int equipment, std::string& note);
    bool ResolveSearchNames(const std::string& search);
    bool ResolveSearchSet();
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
    void ShowQuote(RequestClock::Time now);
    void ClearQuote();
    void RefreshLiveQuote(RequestClock::Time now);
    void RequestQuoteNow();
    void RefreshBuyNote();
    // The selected goods' units for sale: exact from the goods list, or a lower bound after a quote
    // with a full tier list until the goods-list count answers.
    GoodsAvailability SelectedAvailability() const;
    void ApplyGoodsCount();
    void FocusBuyQuantity();
    void ShowLowestMove(RequestClock::Time now);
    void SelectRow(int index);
    void SelectInventory(int slot);
    // Runs a review or a collection now, or once the read in flight is answered (ReviewGate).
    void StartAfterRead(std::function<void()> start);
    void ReviewBuy();
    void ReviewSale();
    void ReviewCancel(const Market::EntityId& id);
    // An empty id collects every entry.
    void Collect(std::optional<Market::EntityId> id);
    void OpenReview(Review review, const Rml::String& name, const Rml::String& title, const Rml::String& action);
    void DismissReview();
    // The open review's balance line follows the wallet (a sale notice or a claim can change it).
    void RefreshReviewBalance();
    void Confirm();
    bool SendAction();
    void StepPrice(PriceStepInput input);
    void EditPrice(bool commit);
    std::optional<Market::Zen> CommitPrice();
    void ChoosePrice(std::optional<Market::Zen> price);
    void ForgetPrice();
    void ShowPrice();
    void RefreshPriceEntry();
    void SaveSellDraft();
    void ClearSellSelection();
    bool AcceptSellCheck();
    void RefreshSellMarket();
    void QueueSellMarketRefresh(RequestClock::Time now);
    Market::Zen SellLowest() const;
    bool EnsurePriceMemory();
    bool RememberPublishedPrice();
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
    RequestClock::Time m_SearchAt{}, m_ReadSentAt{}, m_LastInventory{}, m_LastExpiry{}, m_ActionSentAt{};
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
    // The query of each Browse category and a running market comparison (D9, D10).
    BrowseState m_Browse;
    std::vector<std::uint16_t> m_GoodsTypes;
    std::vector<Market::ListingRow> m_Listings;
    std::vector<Market::GoodsRow> m_Goods;
    std::vector<Market::CollectionEntry> m_Collection;
    std::vector<Icon> m_Icons, m_InventoryIcons;
    // The shown item: a Browse row, a listing or the item being priced.
    Icon m_SelectedIcon;
    // The item being priced in Sell, as it was selected; a Browse selection never replaces it (CR-03).
    Item m_SellItem;
    Review m_Review;
    ReviewGate m_ReviewGate;
    StatusLine m_Status;
    NoticeLines m_Notices;
    // A notice arrived while a review was open: the lists refresh when it closes.
    bool m_RefreshAfterReview = false;
    PriceEntry m_PriceEntry;
    std::optional<Market::SellCheck> m_SellCheck;
    std::optional<Market::Quote> m_SellQuote;
    std::optional<Market::PricingIdentity> m_SellIdentity;
    std::unordered_map<std::uint32_t, SellDraft> m_SellDrafts;
    RequestClock::Time m_SellMarketAt{};
    Market::PriceMemory m_PriceMemory;
    std::string m_PriceMemoryOwner;
    std::filesystem::path m_PriceMemoryPath;
    std::optional<RememberedListing> m_PublishedPrice;
    // The live quote of the selected goods row (D7): the shown answer, its change marks and their expiry.
    QuoteRefresh m_QuoteRefresh;
    std::optional<Market::Quote> m_ShownQuote;
    std::vector<TierChange> m_TierChanges;
    Market::Zen m_PreviousLowest = 0;
    RequestClock::Time m_QuoteShownAt{}, m_HighlightUntil{};
    LowestMove m_LowestMove;
    long long m_QuoteAgeSeconds = -1;
    bool m_SelectedCountExact = true;
    std::pair<std::uint16_t, std::uint8_t> m_GoodsCountFor{};
    // The goods row to select again when the goods list reloads after a purchase.
    std::optional<std::pair<std::uint16_t, std::uint8_t>> m_KeepGoods;
};
} // namespace UI::AuctionHouse
