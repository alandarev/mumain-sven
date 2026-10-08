#include "stdafx.h"
#include "UI/AuctionHouse/Window.h"
#include "UI/Core/WindowSystem.h"
#include "Engine/Object/ZzzInfomation.h"

namespace UI::AuctionHouse
{
namespace
{
constexpr std::uint8_t GoodsListingKind = 2;

// An equipment review's short identity: skill, luck, the additional option and every excellent option.
std::string OptionSummary(const RowModel& row)
{
    std::string text = row.summary;
    std::size_t start = 0;
    while (start < row.options.size())
    {
        auto end = row.options.find('\n', start);
        if (end == std::string::npos)
            end = row.options.size();
        if (end > start)
            text += (text.empty() ? "" : " · ") + row.options.substr(start, end - start);
        start = end + 1;
    }
    return text;
}
} // namespace

void Window::OpenReview(Review review, const Rml::String& name, const Rml::String& title, const Rml::String& action)
{
    if (m_View.GetModel().busy)
        return;
    ClearFeedback();
    review.revision = Market::GetState().Revision();
    m_Review = review;
    auto& model = m_View.GetModel();
    m_LowestMove.Clear();
    model.quoteMove.clear();
    model.confirmTitle = title;
    model.confirmName = name;
    model.confirmAction = action;
    model.confirmDetail.clear();
    model.confirmWarning.clear();
    model.confirmTotal = ExactZen(review.amount) + " Zen";
    model.confirmFee = review.action == Action::Publish ? "Posting fee " + ExactZen(review.fee) + " Zen" : "";
    model.review = true;
    RefreshReviewBalance();
    m_ReviewNeedsFocus = true;
    m_Dirty = true;
}

void Window::RefreshReviewBalance()
{
    auto& model = m_View.GetModel();
    std::string balance;
    const auto action = m_Review.action;
    if (model.review && CharacterMachine &&
        (action == Action::BuyItem || action == Action::BuyGoods || action == Action::Publish))
        balance = ReviewBalanceText(action == Action::Publish ? m_Review.fee : m_Review.amount, CharacterMachine->Gold);
    if (balance != model.confirmBalance)
    {
        model.confirmBalance = std::move(balance);
        m_Dirty = true;
    }
}

void Window::StartAfterRead(std::function<void()> start)
{
    m_ReviewGate.Start(m_SentRead != Read::None, std::move(start));
}

void Window::DismissReview()
{
    // A review still waiting for an answer is dropped as well: the player moved on.
    m_ReviewGate.Cancel();
    if (m_View.GetModel().busy)
        return;
    m_ReviewNeedsFocus = false;
    m_View.GetModel().review = false;
    m_View.MarkDirty("review");
    m_Review = {};
    if (m_RefreshAfterReview)
        RefreshListsAfterNotice();
}

void Window::ReviewBuy()
{
    const auto& model = m_View.GetModel();
    if (!model.selected || model.busy || model.review || model.loading || m_Read == Read::Quote || m_Selected < 0)
        return;
    Review review;
    if (model.goods)
    {
        const auto& row = m_Goods.at(m_Selected);
        // An invalid quantity points at its field instead of asking for a quote (D21).
        if (QuantityHint(model.buyQuantity, row.available).error)
        {
            FocusBuyQuantity();
            return;
        }
        const auto& quote = Market::GetState().CurrentQuote();
        const auto quantity = ParseQuantity(model.buyQuantity, std::numeric_limits<std::uint32_t>::max());
        if (!quote || !quantity || !*quantity || quote->type != row.type || quote->level != row.level ||
            quote->requestedQuantity != *quantity || Market::GetState().IsPending(Market::Operation::Quote))
        {
            QueueRead(Read::Quote);
            return;
        }
        if (quote->result != Market::Result::Success || quote->fillableQuantity < *quantity)
        {
            const auto shortfall = ShortfallText(SplitOrder(*quote));
            SetStatus(quote->result == Market::Result::InsufficientStock && !shortfall.empty()
                          ? shortfall + "."
                          : ResultText(quote->result));
            return;
        }
        review.action = Action::BuyGoods;
        review.type = row.type;
        review.level = row.level;
        review.quantity = *quantity;
        review.amount = quote->total;
    }
    else
    {
        if (static_cast<std::size_t>(m_Selected) >= m_Listings.size())
            return;
        const auto& row = m_Listings[m_Selected];
        if (!m_SelectedIcon.item)
        {
            SetStatus("Item data unavailable.");
            return;
        }
        if (row.ownListing)
        {
            SetStatus("This is your listing.");
            return;
        }
        review.action = Action::BuyItem;
        review.id = row.id;
        review.amount = row.price;
        review.quantity = 1;
    }
    if (!CharacterMachine || review.amount > CharacterMachine->Gold)
    {
        SetStatus(ZenShortfallText(review.amount, CharacterMachine ? CharacterMachine->Gold : 0) + ".");
        return;
    }
    review.subject = model.selectedName;
    OpenReview(review, model.selectedName + (model.goods ? " ×" + std::to_string(review.quantity) : ""), "Buy item",
               "Buy");
    if (model.goods && m_Review.action == Action::BuyGoods)
    {
        // The review repeats the split per price; the receipt reports the actual charge.
        std::string lines;
        for (const auto& line : OrderSplitLines(SplitOrder(*Market::GetState().CurrentQuote())))
            lines += (lines.empty() ? "" : "\n") + line;
        auto& shown = m_View.GetModel();
        shown.confirmDetail = std::move(lines);
        shown.confirmTotal = "Total " + ExactZen(review.amount) + " Zen";
    }
    else if (m_Review.action == Action::BuyItem && static_cast<std::size_t>(m_Selected) < model.rows.size())
    {
        auto& shown = m_View.GetModel();
        const auto& row = shown.rows[m_Selected];
        shown.confirmDetail = OptionSummary(row);
        if (row.cannotEquip)
            shown.confirmWarning = "Cannot Equip with this character. You can still buy it.";
    }
}

void Window::ReviewSale()
{
    if (m_View.GetModel().busy || m_View.GetModel().review)
        return;
    CommitPrice();
    RefreshSelling();
    const auto& model = m_View.GetModel();
    if (!model.sellReady || m_SellSlot < 0)
    {
        SetStatus(model.sellNote.empty() ? "Check quantity, price and fees." : model.sellNote);
        return;
    }
    const auto& rules = Market::GetState().GetRules();
    const auto price = ParseZen(model.price, rules.maxPrice),
               quantity = ParseQuantity(model.quantity, std::numeric_limits<std::uint32_t>::max());
    const auto duration = ParseZen(model.durationValue, std::numeric_limits<std::uint8_t>::max());
    if (!price || !quantity || !duration)
        return;
    const auto fee = Market::PostingFee(rules, *quantity, *price);
    if (!fee)
        return;
    Review review;
    review.action = Action::Publish;
    review.amount = *price;
    review.quantity = *quantity;
    review.fee = *fee;
    review.slot = static_cast<std::uint8_t>(m_SellSlot);
    review.itemKey = m_SellKey;
    review.duration = static_cast<std::uint8_t>(*duration);
    review.subject = model.selectedName;
    OpenReview(review, model.selectedName, "Review listing", "Publish listing");
    const auto text = DescribeListingReview(rules, model.sellGoods, *quantity, *price, *fee, review.duration);
    auto& shown = m_View.GetModel();
    shown.confirmTotal = text.total;
    shown.confirmFee = text.fees;
    if (!model.sellGoods && m_SellItem)
        shown.confirmDetail = OptionSummary(Describe(m_SellItem.get(), 0));
}

void Window::ReviewCancel(const Market::EntityId& id)
{
    if (m_View.GetModel().busy || m_View.GetModel().review || m_View.GetModel().tab != "own")
        return;
    const auto rows = Market::GetState().OwnListings();
    const auto found = std::find_if(rows.begin(), rows.end(), [&id](const auto& listing) { return listing.id == id; });
    const auto index = static_cast<std::size_t>(found - rows.begin());
    if (found == rows.end() || index >= m_View.GetModel().rows.size())
        return;
    const auto& listing = *found;
    const auto row = m_View.GetModel().rows[index];
    Review review;
    review.action = Action::Cancel;
    review.id = listing.id;
    review.quantity = listing.remaining;
    review.subject = row.name;
    OpenReview(review, row.name, "Cancel listing", "Cancel listing");
    // The exact unit price, the remaining quantity and the option summary tell same-name listings apart.
    const auto text = DescribeCancelReview(listing, Market::GetState().GetRules().refundPostingFee);
    auto& model = m_View.GetModel();
    model.confirmDetail = listing.kind == GoodsListingKind ? std::string{} : OptionSummary(row);
    model.confirmTotal = text.total;
    model.confirmFee = text.fee;
}

void Window::Collect(std::optional<Market::EntityId> id)
{
    auto& model = m_View.GetModel();
    if (model.busy || model.review || !Market::GetState().IsOpen() || model.tab != "collection")
        return;
    // Collecting only moves what the player already owns, so it acts at once (D13).
    Review review;
    review.action = Action::Claim;
    if (id)
    {
        if (std::none_of(m_Collection.begin(), m_Collection.end(),
                         [&id](const auto& entry) { return entry.id == *id; }))
            return;
        review.id = *id;
    }
    review.revision = Market::GetState().Revision();
    ClearFeedback();
    m_Review = review;
    m_Read = Read::None;
    m_ActionQueued = true;
    model.busy = true;
    m_Dirty = true;
}

void Window::Confirm()
{
    auto& model = m_View.GetModel();
    if (!model.review || model.busy || m_Review.action == Action::None)
        return;
    if (!Market::GetState().IsOpen() || m_Review.revision != Market::GetState().Revision())
    {
        DismissReview();
        SetStatus("Market changed. Review again.");
        return;
    }
    if (m_Review.action == Action::Publish)
    {
        if (!SellItemUnchanged() || m_SellSlot != m_Review.slot || m_SellKey != m_Review.itemKey)
        {
            DismissReview();
            SetStatus("Item changed. Select it again.");
            return;
        }
        m_PublishedPrice.reset();
        if (m_SellIdentity)
            m_PublishedPrice = RememberedListing{*m_SellIdentity, m_Review.amount};
    }
    m_Read = Read::None;
    m_ActionQueued = true;
    model.busy = true;
    m_Dirty = true;
}

bool Window::SendAction()
{
    if (m_Review.revision != Market::GetState().Revision())
        return false;
    const auto& r = m_Review;
    switch (r.action)
    {
    case Action::BuyItem:
        return Market::BuyListing(r.id, r.amount);
    case Action::BuyGoods:
        return Market::BuyGoods(r.type, r.level, r.quantity, r.amount);
    case Action::Publish:
    {
        const auto* item = InventoryItem(r.slot);
        return item && item->Key == r.itemKey && SellItemUnchanged() &&
               Market::CreateListing(r.slot, r.quantity, r.amount, r.duration, r.fee);
    }
    case Action::Cancel:
        return Market::CancelListing(r.id);
    case Action::Claim:
        return Market::Claim(r.id);
    default:
        return false;
    }
}
} // namespace UI::AuctionHouse
