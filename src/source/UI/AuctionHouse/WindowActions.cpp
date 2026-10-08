#include "stdafx.h"
#include "UI/AuctionHouse/Window.h"
#include "UI/Core/WindowSystem.h"
#include "Engine/Object/ZzzInfomation.h"

namespace UI::AuctionHouse
{
void Window::OpenReview(Review review, const Rml::String& name, const Rml::String& title, const Rml::String& action)
{
    if (m_View.GetModel().busy)
        return;
    SetStatus({});
    review.revision = Market::GetState().Revision();
    m_Review = review;
    auto& model = m_View.GetModel();
    model.confirmTitle = title;
    model.confirmName = name;
    model.confirmAction = action;
    model.confirmTotal = ExactZen(review.amount) + " Zen";
    model.confirmFee = review.action == Action::Publish ? "Posting fee " + ExactZen(review.fee) + " Zen" : "";
    model.confirmBalance.clear();
    if (CharacterMachine &&
        (review.action == Action::BuyItem || review.action == Action::BuyGoods || review.action == Action::Publish))
    {
        const auto debit = review.action == Action::Publish ? review.fee : review.amount;
        if (debit <= CharacterMachine->Gold)
            model.confirmBalance = "Balance " + ExactZen(CharacterMachine->Gold - debit) + " Zen";
    }
    model.review = true;
    m_ReviewNeedsFocus = true;
    m_Dirty = true;
}

void Window::DismissReview()
{
    if (m_View.GetModel().busy)
        return;
    m_ReviewNeedsFocus = false;
    m_View.GetModel().review = false;
    m_View.MarkDirty("review");
    if (!m_View.GetModel().busy)
        m_Review = {};
}

void Window::ReviewBuy()
{
    const auto& model = m_View.GetModel();
    if (!model.selected || model.busy || model.review || model.loading || m_Read == Read::Quote || m_Selected < 0)
        return;
    Review review;
    if (model.goods)
    {
        const auto& quote = Market::GetState().CurrentQuote();
        const auto quantity = ParseQuantity(model.buyQuantity, std::numeric_limits<std::uint32_t>::max());
        const auto& row = m_Goods.at(m_Selected);
        if (!quote || !quantity || !*quantity || quote->type != row.type || quote->level != row.level ||
            quote->requestedQuantity != *quantity || quote->fillableQuantity < *quantity ||
            quote->result != Market::Result::Success || Market::GetState().IsPending(Market::Operation::Quote))
        {
            QueueRead(Read::Quote);
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
        SetStatus("Not enough Zen.");
        return;
    }
    OpenReview(review, model.selectedName + (model.goods ? " ×" + std::to_string(review.quantity) : ""), "Buy item",
               "Buy");
}

void Window::ReviewSale()
{
    if (m_View.GetModel().busy || m_View.GetModel().review)
        return;
    RefreshSelling();
    const auto& model = m_View.GetModel();
    if (!model.sellReady || m_SellSlot < 0)
    {
        SetStatus("Check quantity, price and fees.");
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
    OpenReview(review, model.selectedName + (model.sellGoods ? " ×" + std::to_string(*quantity) : ""), "Review listing",
               "Publish listing");
    m_View.GetModel().confirmTotal = "Total " + model.saleTotal;
    m_View.GetModel().confirmFee = "Posting fee " + model.postingFee + "\nSale fee " + model.saleFee +
                                   "\nNet proceeds " + model.proceeds + "\nDuration " + std::to_string(*duration) +
                                   " h";
}

void Window::ReviewCancel(int index)
{
    if (m_View.GetModel().busy || m_View.GetModel().review)
        return;
    const auto rows = Market::GetState().OwnListings();
    if (index < 0 || static_cast<std::size_t>(index) >= rows.size())
        return;
    Review review;
    review.action = Action::Cancel;
    review.id = rows[index].id;
    review.quantity = rows[index].remaining;
    OpenReview(review, m_View.GetModel().rows[index].name, "Cancel listing", "Cancel listing");
    m_View.GetModel().confirmTotal = "Remaining ×" + std::to_string(review.quantity);
    m_View.GetModel().confirmFee =
        Market::GetState().GetRules().refundPostingFee ? "Posting fee refunded" : "Posting fee kept";
}

void Window::ReviewClaim(int index)
{
    if (m_View.GetModel().busy || m_View.GetModel().review)
        return;
    Review review;
    review.action = Action::Claim;
    Rml::String name = "All entries";
    if (index >= 0)
    {
        if (static_cast<std::size_t>(index) >= m_Collection.size())
            return;
        review.id = m_Collection[index].id;
        review.amount = m_Collection[index].net;
        name = m_View.GetModel().rows[index].name;
    }
    OpenReview(review, name, "Collect", "Collect");
    if (index < 0 || !review.amount)
        m_View.GetModel().confirmTotal.clear();
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
