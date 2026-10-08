#include "stdafx.h"
#include "UI/AuctionHouse/Window.h"

#include <limits>

namespace UI::AuctionHouse
{
namespace
{
constexpr auto ChangeHighlight = std::chrono::seconds(3);
constexpr auto AnyQuantity = std::numeric_limits<std::uint32_t>::max();
} // namespace

void Window::RefreshQuote()
{
    auto& model = m_View.GetModel();
    const auto& quote = Market::GetState().CurrentQuote();
    if (!quote || m_Selected < 0 || static_cast<std::size_t>(m_Selected) >= m_Goods.size())
        return;
    auto& goods = m_Goods[m_Selected];
    const auto quantity = ParseQuantity(model.buyQuantity, AnyQuantity);
    if (!quantity || quote->type != goods.type || quote->level != goods.level || quote->requestedQuantity != *quantity)
        return;
    const auto now = RequestClock::Time::clock::now();
    // The next live refresh counts from this answer.
    m_QuoteRefresh.Schedule(now);
    if (quote->result == Market::Result::Busy)
    {
        m_Clock.Busy(now);
        return;
    }
    if (quote->result != Market::Result::Success && quote->result != Market::Result::InsufficientStock)
    {
        SetStatus(ResultText(quote->result));
        m_Dirty = true;
        return;
    }
    if (m_ShownQuote && m_ShownQuote->type == quote->type && m_ShownQuote->level == quote->level)
    {
        auto changes = CompareTiers(m_ShownQuote->tiers, quote->tiers);
        const auto before = m_ShownQuote->tiers.empty() ? 0 : m_ShownQuote->tiers.front().unitPrice;
        const auto after = quote->tiers.empty() ? 0 : quote->tiers.front().unitPrice;
        const bool lowestMoved = before && after && before != after;
        if (AnyTierChange(changes) || lowestMoved)
        {
            m_TierChanges = std::move(changes);
            m_PreviousLowest = lowestMoved ? before : 0;
            m_HighlightUntil = now + ChangeHighlight;
        }
    }
    else
    {
        m_TierChanges.clear();
        m_PreviousLowest = 0;
        m_HighlightUntil = {};
    }
    m_ShownQuote = *quote;
    m_QuoteShownAt = now;
    m_QuoteAgeSeconds = -1;
    if (!quote->tiers.empty())
        goods.lowestUnitPrice = quote->tiers.front().unitPrice;
    ShowQuote(now);
    if (m_Review.action == Action::BuyGoods && !model.busy)
    {
        // The revised quote is displayed, but never sent until a new explicit review.
        DismissReview();
        SetStatus("Price changed. Review the new total.");
    }
}

void Window::ShowQuote(RequestClock::Time now)
{
    auto& model = m_View.GetModel();
    model.quoteTiers.clear();
    model.quoteTotal.clear();
    model.quoteOrder.clear();
    model.quoteBeyond.clear();
    model.quoteShort.clear();
    m_Dirty = true;
    if (!m_ShownQuote)
        return;
    const auto& quote = *m_ShownQuote;
    const bool highlight = m_HighlightUntil != RequestClock::Time{} && now < m_HighlightUntil;
    const auto split = SplitOrder(quote);
    for (std::size_t i = 0; i < split.tiers.size(); ++i)
    {
        const auto& tier = split.tiers[i];
        TierModel row{CompactZen(tier.unitPrice) + " Zen", std::to_string(tier.stock),
                      tier.inOrder ? std::to_string(tier.inOrder) : "–"};
        if (highlight && i < m_TierChanges.size())
        {
            const auto& change = m_TierChanges[i];
            if (change.added)
                row.note = "new";
            else if (change.stockChanged)
                row.stock = StockChangeText(change.previousStock, tier.stock);
            row.changed = change.added || change.stockChanged;
        }
        model.quoteTiers.push_back(std::move(row));
    }
    model.quoteOrder = OrderSummaryText(split);
    model.quoteBeyond = BeyondTiersText(split);
    model.quoteShort = ShortfallText(split);
    if (split.fillable)
        model.quoteTotal = ExactZen(quote.total) + " Zen";
    if (m_Selected < 0 || static_cast<std::size_t>(m_Selected) >= m_Goods.size() ||
        static_cast<std::size_t>(m_Selected) >= model.rows.size())
        return;
    // Only the selected row changes, in place: the list, its icons and its scroll position stay.
    const auto& goods = m_Goods[m_Selected];
    auto& row = model.rows[m_Selected];
    row.changed = highlight && m_PreviousLowest != 0;
    row.price =
        (row.changed ? PriceChangeText(m_PreviousLowest, goods.lowestUnitPrice) : CompactZen(goods.lowestUnitPrice)) +
        " Zen / each";
    row.exactPrice = ExactZen(goods.lowestUnitPrice) + " Zen / each";
    m_View.MarkDirty("rows");
}

void Window::ClearQuote()
{
    auto& model = m_View.GetModel();
    m_QuoteRefresh.Stop();
    m_ShownQuote.reset();
    m_TierChanges.clear();
    m_PreviousLowest = 0;
    m_HighlightUntil = {};
    m_QuoteAgeSeconds = -1;
    model.quoteTiers.clear();
    model.quoteTotal.clear();
    model.quoteOrder.clear();
    model.quoteBeyond.clear();
    model.quoteShort.clear();
    model.quoteAge.clear();
    m_Dirty = true;
}

void Window::RefreshLiveQuote(RequestClock::Time now)
{
    auto& model = m_View.GetModel();
    const bool watching = model.tab == "browse" && model.goods && model.selected && m_Selected >= 0 &&
                          static_cast<std::size_t>(m_Selected) < m_Goods.size();
    if (!watching || !m_ShownQuote)
        return;
    const auto age = std::chrono::duration_cast<std::chrono::seconds>(now - m_QuoteShownAt);
    if (age.count() != m_QuoteAgeSeconds)
    {
        m_QuoteAgeSeconds = age.count();
        model.quoteAge = QuoteAgeText(age);
        m_View.MarkDirty("quote_age");
    }
    if (m_HighlightUntil != RequestClock::Time{} && now >= m_HighlightUntil)
    {
        m_HighlightUntil = {};
        ShowQuote(now);
    }
    // Paused while a review is open, a request is queued or in flight, or the window is busy; the
    // window being closed stops Update altogether.
    const bool paused = model.busy || model.review || model.filtersOpen || model.loading || m_ActionQueued ||
                        m_Read != Read::None || m_SentRead != Read::None;
    if (!m_QuoteRefresh.Due(now, paused))
        return;
    m_QuoteRefresh.Schedule(now);
    if (ParseQuantity(model.buyQuantity, AnyQuantity))
        m_Read = Read::Quote; // No loading state: the table stays as it is until the answer.
}

void Window::RequestQuoteNow()
{
    auto& model = m_View.GetModel();
    if (model.busy || model.review || !model.goods || m_Selected < 0 ||
        static_cast<std::size_t>(m_Selected) >= m_Goods.size() || !ParseQuantity(model.buyQuantity, AnyQuantity))
        return;
    SetStatus({});
    m_QuoteRefresh.Schedule(RequestClock::Time::clock::now());
    if (m_Read == Read::None)
        m_Read = Read::Quote;
}
} // namespace UI::AuctionHouse
