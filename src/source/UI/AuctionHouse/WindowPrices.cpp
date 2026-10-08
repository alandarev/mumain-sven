#include "stdafx.h"
#include "UI/AuctionHouse/Window.h"
#include "UI/AuctionHouse/ItemPresentation.h"
#include "UI/AuctionHouse/PriceInput.h"
#include "Core/Utilities/StringUtils.h"
#include "Network/Server/ServerListManager.h"
#include "Network/Server/WSclient.h"
#include "Scenes/SceneCore.h"

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>

namespace UI::AuctionHouse
{
namespace
{
constexpr std::uint8_t GoodsListingKind = 2;
constexpr auto SellMarketRefresh = std::chrono::seconds(5);
} // namespace

void Window::BindPriceEvents(Rml::DataModelConstructor& c)
{
    auto bind = [this, &c](const char* name, std::function<void(Rml::Event&)> handler)
    {
        c.BindEventCallback(name, [handler = std::move(handler)](Rml::DataModelHandle, Rml::Event& event,
                                                                 const Rml::VariantList&) { handler(event); });
    };
    bind("price_change",
         [this](Rml::Event& event)
         {
             const bool commit = event.GetParameter<bool>("linebreak", false);
             m_Events.push_back([this, commit] { EditPrice(commit); });
         });
    bind("price_commit", [this](Rml::Event&) { m_Events.push_back([this] { EditPrice(true); }); });
    bind("price_key",
         [this](Rml::Event& event)
         {
             if (const auto step = PriceStepFromKey(event))
             {
                 event.StopPropagation();
                 m_Events.push_back([this, step = *step] { StepPrice(step); });
             }
         });
    bind("price_wheel",
         [this](Rml::Event& event)
         {
             // The wheel steps only over the − and + buttons; there nothing scrolls.
             if (const auto step = PriceStepFromWheel(event))
             {
                 event.StopPropagation();
                 m_Events.push_back([this, step = *step] { StepPrice(step); });
             }
         });
    bind("minus", [this](Rml::Event& event)
         { m_Events.push_back([this, steps = PriceStepsFromClick(event)] { StepPrice({false, steps}); }); });
    bind("plus", [this](Rml::Event& event)
         { m_Events.push_back([this, steps = PriceStepsFromClick(event)] { StepPrice({true, steps}); }); });
    bind("select_content",
         [this](Rml::Event& event)
         {
             // Selecting after the click has placed the cursor keeps the selection, so typing replaces it.
             auto* field = event.GetCurrentElement();
             const auto id = field ? field->GetId() : Rml::String{};
             m_Events.push_back(
                 [this, id]
                 {
                     if (auto* document = m_View.Document(); document && !id.empty())
                         SelectContentIfFocused(document->GetElementById(id));
                 });
         });
    bind("match_lowest",
         [this](Rml::Event&)
         {
             m_Events.push_back(
                 [this]
                 {
                     const auto lowest = SellLowest();
                     ChoosePrice(lowest ? std::optional<Market::Zen>(lowest) : std::nullopt);
                 });
         });
    bind("undercut", [this](Rml::Event&)
         { m_Events.push_back([this] { ChoosePrice(UndercutPrice(Market::GetState().GetRules(), SellLowest())); }); });
    bind("forget_price", [this](Rml::Event&) { m_Events.push_back([this] { ForgetPrice(); }); });
    bind("max_quantity",
         [this](Rml::Event&)
         {
             m_Events.push_back(
                 [this]
                 {
                     auto& model = m_View.GetModel();
                     if (model.busy || !m_SellCheck || m_SellCheck->kind != GoodsListingKind)
                         return;
                     model.quantity = std::to_string(m_SellCheck->available);
                     DismissReview();
                     ClearFeedback();
                     SaveSellDraft();
                     RefreshSelling();
                 });
         });
}

void Window::ShowPrice()
{
    m_View.GetModel().price = m_PriceEntry.Text();
    m_Dirty = true;
}

void Window::EditPrice(bool commit)
{
    if (m_SellSlot < 0)
        return;
    if (m_PriceEntry.Edit(m_View.GetModel().price))
    {
        DismissReview();
        ClearFeedback();
        SaveSellDraft();
    }
    if (commit)
        CommitPrice();
    RefreshSelling();
}

std::optional<Market::Zen> Window::CommitPrice()
{
    if (m_SellSlot < 0)
        return std::nullopt;
    m_PriceEntry.Edit(m_View.GetModel().price);
    const auto price = m_PriceEntry.Commit(Market::GetState().GetRules());
    ShowPrice();
    SaveSellDraft();
    return price;
}

void Window::StepPrice(PriceStepInput input)
{
    if (m_View.GetModel().busy || m_SellSlot < 0 || !m_SellCheck)
        return;
    m_PriceEntry.Edit(m_View.GetModel().price);
    if (m_PriceEntry.Step(Market::GetState().GetRules(), input, SellLowest()))
    {
        ShowPrice();
        SaveSellDraft();
        DismissReview();
        ClearFeedback();
    }
    RefreshSelling();
}

void Window::ChoosePrice(std::optional<Market::Zen> price)
{
    if (!price || m_View.GetModel().busy || m_SellSlot < 0)
        return;
    m_PriceEntry.Choose(Market::GetState().GetRules(), *price);
    ShowPrice();
    SaveSellDraft();
    DismissReview();
    ClearFeedback();
    RefreshSelling();
}

void Window::ForgetPrice()
{
    if (!m_SellIdentity || m_View.GetModel().busy || !EnsurePriceMemory())
        return;
    if (m_PriceMemory.Forget(*m_SellIdentity) && !m_PriceMemory.Save(m_PriceMemoryPath, m_PriceMemoryOwner))
        SetStatus("Could not save the remembered prices.");
    if (m_PriceEntry.Source() == PriceSource::Remembered)
    {
        m_PriceEntry.Reset();
        m_PriceEntry.Prefill(Market::GetState().GetRules(), std::nullopt, std::nullopt, SellLowest());
        ShowPrice();
    }
    RefreshSelling();
}

void Window::RefreshPriceEntry()
{
    auto& model = m_View.GetModel();
    const auto& rules = Market::GetState().GetRules();
    const auto lowest = SellLowest();
    const auto steps = m_PriceEntry.Steps(rules, lowest);
    const bool ready = m_SellSlot >= 0 && m_SellCheck.has_value();
    model.stepDown = ready && steps.down;
    model.stepUp = ready && steps.up;
    model.priceSource = Rml::String(PriceSourceLabel(m_PriceEntry.Source()));
    model.priceNote = m_PriceEntry.Note();
    if (model.priceNote.empty() && ready && (!steps.down || !steps.up))
        model.priceNote = steps.reason;
    model.canMatch = ready && lowest != 0 && Market::RoundPriceDown(rules, lowest).has_value();
    model.canUndercut = ready && UndercutPrice(rules, lowest).has_value();
    model.canForget = m_SellIdentity && EnsurePriceMemory() && m_PriceMemory.Find(*m_SellIdentity).has_value();
}

void Window::SaveSellDraft()
{
    if (m_SellSlot < 0 || m_SellKey == 0)
        return;
    const auto& model = m_View.GetModel();
    if (!m_PriceEntry.Edited() && model.quantity == "1")
    {
        m_SellDrafts.erase(m_SellKey);
        return;
    }
    m_SellDrafts[m_SellKey] = {m_PriceEntry.Edited() ? m_PriceEntry.Text() : Rml::String{}, model.quantity};
}

void Window::ClearSellSelection()
{
    auto& model = m_View.GetModel();
    m_SellSlot = -1;
    m_SellKey = 0;
    m_SellCheck.reset();
    m_SellQuote.reset();
    m_SellIdentity.reset();
    m_PriceEntry.Reset();
    model.selected = false;
    model.sellGoods = false;
    model.price.clear();
    model.quantity = "1";
    model.maxQuantity.clear();
    model.marketLowest.clear();
    model.sellTiers.clear();
    model.depthLabel.clear();
    MarkSelectedInventory(model.inventory, m_SellSlot);
    RefreshPriceEntry();
    m_Dirty = true;
}

Market::Zen Window::SellLowest() const
{
    if (!m_SellCheck || m_SellCheck->kind != GoodsListingKind || m_SellCheck->result != Market::Result::Success)
        return 0;
    return m_SellCheck->lowestUnitPrice;
}

bool Window::AcceptSellCheck()
{
    const auto& state = Market::GetState();
    const auto& check = state.LastSellCheck();
    if (!check || check->inventorySlot != m_SellSlot || state.IsPending(Market::Operation::SellCheck))
        return false;
    if (m_SellCheck)
    {
        // A refresh: a failed one keeps the last answer.
        if (m_SellCheck->requestId == check->requestId || check->result != Market::Result::Success)
            return false;
    }
    else if (m_Read == Read::Sell || m_SentRead == Read::Sell)
        return false; // The answer still belongs to an earlier selection.
    m_SellCheck = *check;
    auto& model = m_View.GetModel();
    model.sellGoods = check->kind == GoodsListingKind;
    model.stock = std::to_string(check->available) + " available";
    model.maxQuantity = model.sellGoods ? "Max (" + std::to_string(check->available) + ")" : "";
    if (!model.sellGoods)
        model.quantity = "1";
    if (check->result != Market::Result::Success || check->reason != 0)
        return true;
    if (!m_SellIdentity)
    {
        if (model.sellGoods)
            m_SellIdentity = Market::GoodsPricingIdentity(check->type, check->level);
        else if (m_SelectedIcon.item)
            m_SellIdentity = EquipmentPricingIdentity(*m_SelectedIcon.item);
    }
    std::optional<Market::Zen> remembered;
    if (m_SellIdentity && EnsurePriceMemory())
        remembered = m_PriceMemory.Find(*m_SellIdentity);
    // Typing that has not been handled yet still counts as an edit, so a late answer cannot replace it.
    m_PriceEntry.Edit(model.price);
    if (m_PriceEntry.Prefill(state.GetRules(), std::nullopt, remembered, SellLowest()))
        ShowPrice();
    RefreshSellMarket();
    if (model.sellGoods)
        QueueRead(Read::SellQuote);
    m_SellMarketAt = RequestClock::Time::clock::now() + SellMarketRefresh;
    return true;
}

void Window::RefreshSellMarket()
{
    auto& model = m_View.GetModel();
    const auto& quote = Market::GetState().CurrentQuote();
    if (quote && m_SellCheck && quote->type == m_SellCheck->type && quote->level == m_SellCheck->level)
        m_SellQuote = *quote;
    model.sellTiers.clear();
    model.marketLowest.clear();
    model.depthLabel.clear();
    m_Dirty = true;
    if (!model.sellGoods || !m_SellCheck)
        return;
    const auto lowest = SellLowest();
    const auto tiers =
        m_SellQuote ? std::span<const Market::PriceTier>(m_SellQuote->tiers) : std::span<const Market::PriceTier>{};
    model.marketLowest = MarketLowestText(lowest, tiers);
    if (lowest == 0)
        return;
    for (const auto& tier : tiers)
        model.sellTiers.push_back({ExactZen(tier.unitPrice), std::to_string(tier.quantity)});
    model.depthLabel = MarketDepthLabel(model.sellTiers.size());
}

void Window::QueueSellMarketRefresh(RequestClock::Time now)
{
    const auto& model = m_View.GetModel();
    if (!m_SellCheck || !model.sellGoods || model.busy || model.review || m_Read != Read::None ||
        m_SentRead != Read::None || now < m_SellMarketAt)
        return;
    m_SellMarketAt = now + SellMarketRefresh;
    QueueRead(Read::Sell);
}

bool Window::EnsurePriceMemory()
{
    if (LogInID[0] == L'\0')
        return false;
    const auto owner =
        Market::PriceMemoryOwner(szServerIpAddress ? StringUtils::WideToNarrow(szServerIpAddress) : std::string{},
                                 g_ServerPort, StringUtils::WideToNarrow(g_ServerListManager->GetSelectServerName()),
                                 g_ServerListManager->GetSelectServerIndex(), StringUtils::WideToNarrow(LogInID));
    if (owner == m_PriceMemoryOwner)
        return true;
    m_PriceMemoryOwner = owner;
    m_PriceMemoryPath = Market::PriceMemoryPath(owner);
    // A missing or unreadable file starts an empty memory; the next publication rewrites it.
    m_PriceMemory.Load(m_PriceMemoryPath, owner);
    return true;
}

bool Window::RememberPublishedPrice()
{
    const auto published = std::exchange(m_PublishedPrice, std::nullopt);
    if (!published || !EnsurePriceMemory())
        return true;
    m_PriceMemory.Remember(published->identity, published->price);
    return m_PriceMemory.Save(m_PriceMemoryPath, m_PriceMemoryOwner);
}
} // namespace UI::AuctionHouse
