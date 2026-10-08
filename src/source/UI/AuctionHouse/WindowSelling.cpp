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
    model.saleTotal.clear();
    model.postingFee.clear();
    model.saleFee.clear();
    model.proceeds.clear();
    if (m_SellSlot < 0)
    {
        RefreshPriceEntry();
        m_Dirty = true;
        return;
    }
    if (!SellItemUnchanged())
    {
        ClearSellSelection();
        DismissReview();
        SetStatus("Item changed. Select it again.");
        return;
    }
    AcceptSellCheck();
    RefreshPriceEntry();
    m_Dirty = true;
    if (!m_SellCheck)
        return;
    const auto& check = *m_SellCheck;
    if (check.result != Market::Result::Success || check.reason != 0)
    {
        SetStatus(check.result != Market::Result::Success ? ResultText(check.result) : "This item cannot be listed.");
        return;
    }
    const auto& rules = Market::GetState().GetRules();
    const auto quantity = ParseQuantity(model.quantity, check.available);
    // Fees are shown for the price that a commit lists: the typed price rounded down to a valid one.
    const auto typed = ParseZen(model.price, rules.maxPrice);
    const auto price = typed ? Market::RoundPriceDown(rules, *typed) : std::nullopt;
    if (!quantity || !price)
        return;
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
    if (check.activeListings >= rules.maxActiveListings)
        SetStatus("Listing limit reached.");
    else if (!CharacterMachine || *fee > CharacterMachine->Gold)
        SetStatus("Not enough Zen for the posting fee.");
    model.sellReady = !model.busy && check.activeListings < rules.maxActiveListings && CharacterMachine &&
                      *fee <= CharacterMachine->Gold;
}
} // namespace UI::AuctionHouse
