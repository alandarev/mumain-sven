#include "stdafx.h"
#include "UI/AuctionHouse/Window.h"
#include "UI/Core/WindowSystem.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Core/Utilities/StringUtils.h"

namespace UI::AuctionHouse
{
void Window::RefreshRules()
{
    auto& model = m_View.GetModel();
    const auto& rules = Market::GetState().GetRules();
    model.durations.clear();
    for (auto hours : rules.durations)
        model.durations.push_back({std::to_string(hours), std::to_string(hours) + " h"});
    if (std::none_of(model.durations.begin(), model.durations.end(),
                     [&model](const auto& duration) { return duration.value == model.durationValue; }))
        model.durationValue = model.durations.empty() ? "" : model.durations.front().value;
    model.duration = model.durations.size() == 1 ? model.durations.front().label : "";
    model.pending = rules.pendingCollection;
    if (model.sets.empty())
    {
        model.sets.push_back({"", "Any set"});
        for (const auto& set : Market::AncientSets())
        {
            const auto name = StringUtils::WideToNarrow(set.c_str());
            model.sets.push_back({name, name});
        }
    }
    m_Wallet = CharacterMachine ? CharacterMachine->Gold : 0;
    if (CharacterMachine)
        model.wallet = CompactZen(static_cast<Market::Zen>(CharacterMachine->Gold)) + " Zen";
    RefreshSelling();
    m_Dirty = true;
}

void Window::RefreshSelling()
{
    auto& model = m_View.GetModel();
    model.sellReady = false;
    model.stepDown = model.stepUp = false;
    model.saleTotal.clear();
    model.postingFee.clear();
    model.saleFee.clear();
    model.proceeds.clear();
    if (m_SellSlot < 0)
    {
        m_Dirty = true;
        return;
    }
    if (!SellItemUnchanged())
    {
        m_SellSlot = -1;
        m_SellKey = 0;
        model.selected = false;
        DismissReview();
        SetStatus("Item changed. Select it again.");
        return;
    }
    if (m_Read == Read::Sell || m_SentRead == Read::Sell)
        return;
    const auto& state = Market::GetState();
    const auto& check = state.LastSellCheck();
    if (!check || check->inventorySlot != m_SellSlot || state.IsPending(Market::Operation::SellCheck))
        return;
    if (check->result != Market::Result::Success || check->reason != 0)
    {
        SetStatus(check->result != Market::Result::Success ? ResultText(check->result) : "This item cannot be listed.");
        return;
    }
    model.sellGoods = check->kind == 2;
    model.stock = std::to_string(check->available) + " available";
    if (!model.sellGoods)
        model.quantity = "1";
    const auto& rules = state.GetRules();
    const auto quantity = ParseQuantity(model.quantity, check->available);
    const auto price = ParseZen(model.price, rules.maxPrice);
    if (!quantity || !*quantity || !price || !*price)
    {
        m_Dirty = true;
        return;
    }
    model.stepDown = model.sellGoods && Market::PreviousGoodsPrice(rules, *price).has_value();
    model.stepUp = model.sellGoods && Market::NextGoodsPrice(rules, *price).has_value();
    const auto total = static_cast<std::uint64_t>(*quantity) * *price;
    const auto fee = Market::PostingFee(rules, *quantity, *price);
    if (!fee || total > rules.zenLimit)
    {
        SetStatus("Price exceeds the market limit.");
        return;
    }
    const auto saleFee = Market::SaleFee(rules, static_cast<Market::Zen>(total));
    model.saleTotal = ExactZen(static_cast<Market::Zen>(total)) + " Zen";
    model.postingFee = ExactZen(*fee) + " Zen";
    model.saleFee = ExactZen(saleFee) + " Zen";
    model.proceeds = ExactZen(static_cast<Market::Zen>(total) - saleFee) + " Zen";
    if (check->activeListings >= rules.maxActiveListings)
        SetStatus("Listing limit reached.");
    else if (model.sellGoods && !Market::GoodsPriceOnStep(rules, *price))
        SetStatus("Choose a price using − or +.");
    else if (!CharacterMachine || *fee > CharacterMachine->Gold)
        SetStatus("Not enough Zen for the posting fee.");
    model.sellReady = !model.busy && check->activeListings < rules.maxActiveListings &&
                      (!model.sellGoods || Market::GoodsPriceOnStep(rules, *price)) && CharacterMachine &&
                      *fee <= CharacterMachine->Gold;
    m_Dirty = true;
}

void Window::StepPrice(bool up)
{
    auto& model = m_View.GetModel();
    if (!model.sellGoods || model.busy)
        return;
    const auto& rules = Market::GetState().GetRules();
    const auto price = ParseZen(model.price, rules.maxPrice);
    if (!price || !*price)
        return;
    const auto stepped = up ? Market::NextGoodsPrice(rules, *price) : Market::PreviousGoodsPrice(rules, *price);
    if (!stepped)
        return;
    model.price = std::to_string(*stepped);
    DismissReview();
    SetStatus({});
    RefreshSelling();
}

void Window::RefreshQuote()
{
    auto& model = m_View.GetModel();
    model.quoteTotal.clear();
    const auto& quote = Market::GetState().CurrentQuote();
    if (!quote || m_Selected < 0 || static_cast<std::size_t>(m_Selected) >= m_Goods.size())
        return;
    const auto& goods = m_Goods[m_Selected];
    const auto quantity = ParseQuantity(model.buyQuantity, std::numeric_limits<std::uint32_t>::max());
    if (!quantity || quote->type != goods.type || quote->level != goods.level || quote->requestedQuantity != *quantity)
        return;
    model.stock = std::to_string(quote->result == Market::Result::Success ? goods.available : quote->fillableQuantity) +
                  " available";
    if (quote->result != Market::Result::Success || quote->fillableQuantity < *quantity)
    {
        SetStatus(ResultText(quote->result));
        m_Dirty = true;
        return;
    }
    model.quoteTotal = ExactZen(quote->total) + " Zen";
    model.selectedDetail.clear();
    for (const auto& tier : quote->tiers)
    {
        if (!model.selectedDetail.empty())
            model.selectedDetail += "\n";
        model.selectedDetail += ExactZen(tier.unitPrice) + " Zen ×" + std::to_string(tier.quantity);
    }
    if (m_Review.action == Action::BuyGoods && !model.busy)
    {
        // The revised quote is displayed, but never sent until a new explicit review.
        DismissReview();
        SetStatus("Price changed. Review the new total.");
    }
    m_Dirty = true;
}
} // namespace UI::AuctionHouse
