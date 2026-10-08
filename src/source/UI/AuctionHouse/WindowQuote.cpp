#include "stdafx.h"
#include "UI/AuctionHouse/Window.h"
#include "Engine/Object/ZzzInfomation.h"
#include "UI/AuctionHouse/PriceInput.h"

#include <RmlUi/Core/ElementDocument.h>

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
        if (lowestMoved)
        {
            m_LowestMove.Show(before, after, now);
            model.quoteMove = m_LowestMove.Text();
            m_View.MarkDirty("quote_move");
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
    // "N available" follows every refreshed quote, not the goods list it was opened from. A full tier
    // list gives a lower bound, and the goods-list row of this type follows for the exact count.
    const auto availability = AvailableAfterQuote(*quote);
    goods.available = availability.count;
    m_SelectedCountExact = availability.exact;
    if (GoodsCountNeeded(availability) && m_Read == Read::None)
        m_Read = Read::GoodsCount;
    ShowQuote(now);
    RefreshBuyNote();
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
        TierModel row{CompactZen(tier.unitPrice), std::to_string(tier.stock),
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
        " Zen each";
    row.exactPrice = ExactZen(goods.lowestUnitPrice) + " Zen each";
    row.meta = AvailabilityText(SelectedAvailability());
    m_View.MarkDirty("rows");
}

GoodsAvailability Window::SelectedAvailability() const
{
    if (m_Selected < 0 || static_cast<std::size_t>(m_Selected) >= m_Goods.size())
        return {};
    return {m_Goods[m_Selected].available, m_SelectedCountExact};
}

void Window::ApplyGoodsCount()
{
    m_SentRead = Read::None;
    const auto& state = Market::GetState();
    const auto outcome = state.LastOutcome(Market::Operation::GoodsList);
    if (outcome && outcome->result == Market::Result::Busy)
        m_Clock.Busy(RequestClock::Time::clock::now());
    // A failed count keeps the lower bound; the next capped quote asks again.
    if (!outcome || outcome->result != Market::Result::Success || m_Selected < 0 ||
        static_cast<std::size_t>(m_Selected) >= m_Goods.size())
        return;
    auto& goods = m_Goods[m_Selected];
    if (goods.type != m_GoodsCountFor.first || goods.level != m_GoodsCountFor.second)
        return;
    goods.available = AvailableFromGoodsList(state.Goods().rows, goods.type, goods.level).count;
    m_SelectedCountExact = true;
    ShowQuote(RequestClock::Time::clock::now());
    RefreshBuyNote();
}

void Window::ClearQuote()
{
    auto& model = m_View.GetModel();
    m_QuoteRefresh.Stop();
    m_ShownQuote.reset();
    m_SelectedCountExact = true;
    m_TierChanges.clear();
    m_PreviousLowest = 0;
    m_HighlightUntil = {};
    m_LowestMove.Clear();
    model.quoteMove.clear();
    m_QuoteAgeSeconds = -1;
    model.quoteTiers.clear();
    model.quoteTotal.clear();
    model.quoteOrder.clear();
    model.quoteBeyond.clear();
    model.quoteShort.clear();
    model.quoteAge.clear();
    RefreshBuyNote();
    m_Dirty = true;
}

void Window::RefreshBuyNote()
{
    auto& model = m_View.GetModel();
    FieldHint hint;
    BuyFacts facts;
    facts.wallet = CharacterMachine ? CharacterMachine->Gold : 0;
    const bool browsing = model.tab == "browse" && model.selected && m_Selected >= 0;
    bool known = false;
    if (browsing && model.goods && static_cast<std::size_t>(m_Selected) < m_Goods.size())
    {
        known = true;
        hint = BuyQuantityHint(model.buyQuantity, SelectedAvailability());
        facts.quantityValid = !hint.error;
        const auto quantity = ParseQuantity(model.buyQuantity, AnyQuantity);
        if (m_ShownQuote && m_ShownQuote->fillableQuantity && quantity && m_ShownQuote->requestedQuantity == *quantity)
            facts.total = m_ShownQuote->total;
    }
    else if (browsing && !model.goods && static_cast<std::size_t>(m_Selected) < m_Listings.size())
    {
        known = true;
        facts.quantityValid = true;
        facts.ownListing = m_Listings[m_Selected].ownListing;
        facts.total = m_Listings[m_Selected].price;
    }
    // Without a character the wallet is unknown; ReviewBuy still refuses the purchase.
    if (!CharacterMachine)
        facts.total = 0;
    const auto readiness = known ? DescribeBuyReadiness(facts) : BuyReadiness{};
    if (hint.text != model.buyHint || hint.error != model.buyHintError || readiness.reason != model.buyNote ||
        readiness.blocked != model.buyBlocked)
    {
        model.buyHint = std::move(hint.text);
        model.buyHintError = hint.error;
        model.buyNote = readiness.reason;
        model.buyBlocked = readiness.blocked;
        m_Dirty = true;
    }
}

void Window::FocusBuyQuantity()
{
    auto* document = m_View.Document();
    auto* field = document ? document->GetElementById("buy-quantity") : nullptr;
    if (!field)
        return;
    field->Focus();
    SelectContentIfFocused(field);
}

void Window::ShowLowestMove(RequestClock::Time now)
{
    if (!m_LowestMove.Expire(now))
        return;
    m_View.GetModel().quoteMove.clear();
    m_View.MarkDirty("quote_move");
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
    ClearFeedback();
    m_QuoteRefresh.Schedule(RequestClock::Time::clock::now());
    if (m_Read == Read::None)
        m_Read = Read::Quote;
}
} // namespace UI::AuctionHouse
