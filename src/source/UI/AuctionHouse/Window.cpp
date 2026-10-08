#include "stdafx.h"
#include "UI/AuctionHouse/Window.h"
#include "UI/Core/WindowManager.h"
#include "UI/Core/WindowSystem.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlDraggable.h"
#include "UI/RmlBridge/RmlTooltip.h"
#include "UI/Core/WindowCommon.h"
#include <limits>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>

namespace UI::AuctionHouse
{
namespace
{
constexpr auto SearchDebounce = std::chrono::milliseconds(350);
constexpr auto RequestTimeout = std::chrono::seconds(15);
constexpr auto InventoryRefresh = std::chrono::milliseconds(500);

// Native select popups use layout offsets, not transformed bounds. Keep their
// coordinates identical to the drawn panel while retaining the shared drag helper.
void CenterPanel(Rml::Element* panel)
{
    if (!panel || panel->IsClassSet("dragged"))
        return;
    const auto size = panel->GetBox().GetSize(Rml::BoxArea::Border);
    const auto viewport = panel->GetContext()->GetDimensions();
    const float left = std::max(0.f, (static_cast<float>(viewport.x) - size.x) / 2.f);
    const float top = std::max(0.f, (static_cast<float>(viewport.y) - size.y) / 2.f);
    if (std::abs(panel->GetOffsetLeft() - left) > 0.1f)
        panel->SetProperty("left", std::to_string(left) + "px");
    if (std::abs(panel->GetOffsetTop() - top) > 0.1f)
        panel->SetProperty("top", std::to_string(top) + "px");
}

} // namespace

Window::Window()
    : m_View("auction_house", [this](auto& c, auto& model) { Bind(c, model); },
             {{"Data/Interface/RmlUi/auction_house.rml"}}, {.afterBuild = [this] { Built(); }})
{
    CObject::Show(false);
}

Window::~Window()
{
    Release();
}

bool Window::Create(mu::ui::window::CManager* manager)
{
    if (!manager)
        return false;
    m_Manager = manager;
    manager->AddUIObj(mu::ui::window::INTERFACE_AUCTION_HOUSE, this);
    Market::SetListener(this);
    m_View.Ensure();
    if (Market::GetState().IsOpen())
        OnOpened();
    return true;
}

void Window::Release()
{
    if (!m_Manager)
        return;
    Market::SetListener(nullptr);
    m_View.Hide();
    UI::RmlBridge::Tooltip::Hide(this);
    ClearItems();
    m_View.Release();
    m_Manager->RemoveUIObj(this);
    m_Manager = nullptr;
}

void Window::Built()
{
    auto* doc = m_View.Document();
    if (!doc)
        return;
    auto* panel = doc->GetElementById("panel");
    UI::RmlBridge::MakeDraggable(doc->GetElementById("titlebar"), panel, nullptr, [this]
                                 { UI::RmlBridge::KeepInsideWindow(m_View.Document()->GetElementById("panel")); });
    Dirty();
}

void Window::Show(bool show)
{
    if (show && !Market::GetState().IsOpen())
        return;
    CObject::Show(show);
    if (show)
    {
        m_View.Ensure();
        UI::RmlBridge::SyncDocumentVisibilityInFront(m_View.Document(), true);
        return;
    }
    if (auto* doc = m_View.Document())
        if (auto* focus = doc->GetContext()->GetFocusElement(); focus && focus->GetOwnerDocument() == doc)
            focus->Blur();
    m_View.Hide();
    m_Events.clear();
    m_Read = Read::None;
    m_ActionQueued = false;
    m_View.GetModel().busy = false;
    m_ActionSentAt = {};
    m_RefreshAfterReview = false;
    DismissReview();
    DismissFilters();
    EndComparison();
    for (auto& icon : m_Icons)
        if (icon.target)
            icon.target->SetEnabled(false);
    for (auto& icon : m_InventoryIcons)
        if (icon.target)
            icon.target->SetEnabled(false);
    if (m_SelectedIcon.target)
        m_SelectedIcon.target->SetEnabled(false);
    UI::RmlBridge::Tooltip::Hide(this);
    if (Market::GetState().IsOpen())
        Market::Close();
}

void Window::OnOpened()
{
    const bool first = !IsVisible();
    RefreshRules();
    if (!first)
    {
        DismissReview();
        SetStatus("Fees changed. Review again.");
        return;
    }
    m_Clock.Reset();
    m_Generation++;
    m_SentRead = Read::None;
    m_View.GetModel().tab = "browse";
    FilterChanged();
    Show(true);
}

void Window::OnClosed()
{
    Show(false);
    ClearItems();
    m_Listings.clear();
    m_Goods.clear();
    m_Collection.clear();
    m_View.GetModel().rows.clear();
    m_View.GetModel().inventory.clear();
    m_View.GetModel().loading = false;
    m_SellDrafts.clear();
    m_PublishedPrice.reset();
    m_KeepGoods.reset();
    ClearQuote();
    ClearSellSelection();
    m_View.GetModel().depthOpen = false;
    m_SentRead = Read::None;
    Dirty();
}

bool Window::Update()
{
    if (!IsVisible())
        return true;
    m_View.Ensure();
    UI::RmlBridge::SyncDocumentVisibilityInFront(m_View.Document(), true);
    if (auto* document = m_View.Document())
    {
        auto* panel = document->GetElementById("panel");
        CenterPanel(panel);
        if (panel && panel->IsClassSet("dragged"))
            UI::RmlBridge::KeepInsideWindow(panel);
    }
    if (m_ReviewNeedsFocus)
        if (auto* document = m_View.Document())
            if (auto* button = document->GetElementById("confirm-action"); button && button->IsVisible(true))
            {
                button->Focus();
                m_ReviewNeedsFocus = false;
            }
    while (!m_Events.empty())
    {
        auto event = std::move(m_Events.front());
        m_Events.pop_front();
        event();
        if (!IsVisible())
            return true;
    }
    const auto now = RequestClock::Time::clock::now();
    if (CharacterMachine && m_Wallet != CharacterMachine->Gold)
    {
        m_Wallet = CharacterMachine->Gold;
        m_View.GetModel().wallet = CompactZen(m_Wallet) + " Zen";
        RefreshSelling();
        RefreshBuyNote();
        RefreshReviewBalance();
        m_Dirty = true;
    }
    if (m_View.GetModel().busy && m_ActionSentAt != RequestClock::Time{} && now - m_ActionSentAt > RequestTimeout)
    {
        SetStatus(std::string(OutcomeUnknownText));
        m_ActionSentAt = {};
    }
    if (now - m_LastExpiry >= std::chrono::minutes(1) && !m_View.GetModel().busy)
    {
        m_LastExpiry = now;
        if (m_View.GetModel().tab == "own")
            RefreshRows();
    }

    if (m_SearchAt != RequestClock::Time{} && now >= m_SearchAt)
    {
        m_SearchAt = {};
        if (CompileFilter())
            QueueRead(m_View.GetModel().goods ? Read::Goods : Read::Search, true);
    }
    if (m_SentRead != Read::None && now - m_ReadSentAt > RequestTimeout)
    {
        m_SentRead = Read::None;
        m_View.GetModel().loading = false;
        SetStatus("Request timed out. Refresh to retry.");
    }
    if (m_View.GetModel().tab == "sell" && now - m_LastInventory >= InventoryRefresh)
    {
        m_LastInventory = now;
        RefreshInventory();
        RefreshSelling();
        QueueSellMarketRefresh(now);
    }
    CheckScroll();
    RefreshLiveQuote(now);
    PumpRequests(now);
    SyncIcons();
    ShowLowestMove(now);
    if (m_Dirty)
    {
        Dirty();
        m_Dirty = false;
    }
    return true;
}

bool Window::UpdateMouseEvent()
{
    if (!IsVisible() || !m_View.Document())
        return true;
    auto* hovered = m_View.Document()->GetContext()->GetHoverElement();
    return !hovered || hovered->GetOwnerDocument() != m_View.Document();
}

bool Window::UpdateKeyEvent()
{
    if (!IsVisible())
        return true;
    if (!mu::ui::window::IsPress(VK_ESCAPE))
    {
        auto* document = m_View.Document();
        const auto* focused = document ? document->GetContext()->GetFocusElement() : nullptr;
        return !focused || focused->GetOwnerDocument() != document;
    }
    if (m_View.GetModel().filtersOpen)
        DismissFilters();
    else if (m_View.GetModel().review && !m_View.GetModel().busy)
        DismissReview();
    else
        Show(false);
    return false;
}

void Window::SetStatus(const std::string& text, bool error)
{
    m_Status.Show(text, error ? StatusTone::Error : StatusTone::Info, StatusLink::None);
    ShowStatus();
}

void Window::ShowReceipt(const Receipt& receipt)
{
    m_Status.Show(receipt.text, receipt.tone, receipt.link);
    ShowStatus();
}

void Window::ShowNotices()
{
    auto& model = m_View.GetModel();
    model.notices.clear();
    for (const auto& line : m_Notices.Lines())
        model.notices.push_back({line.text, Rml::String(StatusLinkName(line.link))});
    m_View.MarkDirty("notices");
}

void Window::ClearFeedback()
{
    m_Status.Clear();
    ShowStatus();
    if (m_Notices.Lines().empty())
        return;
    m_Notices.Clear();
    ShowNotices();
}

void Window::ShowStatus()
{
    auto& model = m_View.GetModel();
    model.status = m_Status.Text();
    model.statusError = m_Status.Tone() == StatusTone::Error;
    model.statusReceipt = m_Status.Tone() == StatusTone::Success;
    model.statusLink = Rml::String(StatusLinkName(m_Status.Link()));
    for (const char* field : {"status", "status_error", "status_receipt", "status_link"})
        m_View.MarkDirty(field);
}

void Window::SwitchTab(const Rml::String& tab)
{
    if (m_View.GetModel().busy || m_View.GetModel().review)
        return;
    DismissFilters();
    // Leaving Browse ends a market comparison and gives the player's own queries back.
    if (tab != "browse")
        EndComparison();
    m_SearchAt = {};
    m_View.GetModel().tab = tab;
    m_Generation++;
    m_Selected = -1;
    m_KeepGoods.reset();
    ClearQuote();
    m_View.GetModel().selected = false;
    m_View.GetModel().rows.clear();
    m_View.GetModel().count.clear();
    m_View.GetModel().more = false;
    ClearFeedback();
    if (tab == "browse")
        FilterChanged();
    else if (tab == "own")
        QueueRead(Read::Own, true);
    else if (tab == "collection")
        QueueRead(Read::Collection, true);
    else
    {
        m_Read = Read::None;
        RefreshInventory();
        RefreshSelling();
    }
    m_Dirty = true;
}

void Window::FilterChanged()
{
    if (m_View.GetModel().busy || m_View.GetModel().review)
        return;
    m_Generation++;
    ClearFeedback();
    RefreshFilterSummary();
    RefreshBrowseState();
    m_SearchAt = RequestClock::Time::clock::now() + SearchDebounce;
    m_Read = Read::None;
    m_Selected = -1;
    m_KeepGoods.reset();
    ClearQuote();
    m_View.GetModel().selected = false;
    m_View.GetModel().rows.clear();
    m_View.GetModel().count.clear();
    m_View.GetModel().more = false;
    m_View.GetModel().loading = true;
    DismissReview();
    m_Dirty = true;
}

void Window::QueueRead(Read read, bool restart)
{
    if (m_View.GetModel().busy || m_View.GetModel().review)
        return;
    m_Read = read;
    m_Restart = restart;
    if (restart)
        m_Page = 0;
    m_View.GetModel().loading = true;
    m_Dirty = true;
}

void Window::PumpRequests(RequestClock::Time now)
{
    if (!m_Clock.Ready(now))
        return;
    if (m_ActionQueued)
    {
        if (SendAction())
        {
            m_Clock.Sent(now);
            m_ActionQueued = false;
            m_ActionSentAt = now;
        }
        else
        {
            m_ActionQueued = false;
            m_View.GetModel().busy = false;
            SetStatus("Request could not be sent. Review again.");
        }
        m_Dirty = true;
        return;
    }
    if (m_Read == Read::None || m_SentRead != Read::None)
        return;
    const auto read = m_Read;
    m_Read = Read::None;
    if (!SendRead(read))
    {
        m_View.GetModel().loading = false;
        SetStatus("Request could not be sent. Refresh to retry.");
        m_Dirty = true;
        return;
    }
    m_Clock.Sent(now);
    m_SentRestart = m_Restart;
    m_ReadSentAt = now;
    m_SentRead = read;
    m_SentGeneration = m_Generation;
}

bool Window::SendRead(Read read)
{
    switch (read)
    {
    case Read::Search:
        return Market::RequestSearch(m_Filter, m_Page);
    case Read::Goods:
        return Market::RequestGoodsList(m_GoodsTypes, m_Page);
    case Read::Own:
        return Market::RequestOwnListings();
    case Read::Collection:
        return Market::RequestCollection(m_Page);
    case Read::Sell:
        return m_SellSlot >= 0 && Market::CheckSell(static_cast<std::uint8_t>(m_SellSlot));
    case Read::SellQuote:
        return m_SellCheck && Market::RequestQuote(m_SellCheck->type, m_SellCheck->level, 1);
    case Read::Quote:
    {
        if (m_Selected < 0 || static_cast<std::size_t>(m_Selected) >= m_Goods.size())
            return false;
        const auto quantity = ParseQuantity(m_View.GetModel().buyQuantity, std::numeric_limits<std::uint32_t>::max());
        if (!quantity || !*quantity)
        {
            SetStatus("Enter a whole quantity.");
            return false;
        }
        const auto& row = m_Goods[m_Selected];
        return Market::RequestQuote(row.type, row.level, *quantity);
    }
    default:
        return false;
    }
}

void Window::CheckScroll()
{
    auto& model = m_View.GetModel();
    if (!model.more || model.loading || model.busy || model.review || !m_View.Document())
        return;
    auto* list = m_View.Document()->GetElementById("results");
    if (!list || list->GetClientHeight() <= 0)
        return;
    if (list->GetScrollTop() + list->GetClientHeight() < list->GetScrollHeight() - list->GetClientHeight() / 2)
        return;
    if (model.tab == "browse")
        QueueRead(model.goods ? Read::Goods : Read::Search);
    else if (model.tab == "collection")
        QueueRead(Read::Collection);
}
} // namespace UI::AuctionHouse
