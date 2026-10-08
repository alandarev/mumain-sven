#include "stdafx.h"
#include "UI/AuctionHouse/Window.h"
#include "UI/Core/WindowSystem.h"
#include "Engine/Object/ZzzInfomation.h"

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
    model.quantityHint.clear();
    model.quantityHintError = false;
    model.listingCount.clear();
    model.listingCountFull = false;
    model.sellNote.clear();
    model.canCompare = false;
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
    // Equipment only: goods have their market depth beside the price.
    model.canCompare = m_SellCheck.has_value() && !model.sellGoods;
    RefreshPriceEntry();
    m_Dirty = true;
    if (!m_SellCheck)
        return;
    const auto& check = *m_SellCheck;
    if (check.result != Market::Result::Success)
    {
        SetStatus(ResultText(check.result));
        return;
    }
    if (check.reason != 0)
    {
        model.sellNote = SellCheckReasonText(check.reason);
        return;
    }
    const auto& rules = Market::GetState().GetRules();
    model.listingCount = ListingCountText(check.activeListings, rules.maxActiveListings);
    model.listingCountFull = check.activeListings >= rules.maxActiveListings;
    if (model.sellGoods)
    {
        const auto hint = QuantityHint(model.quantity, check.available);
        model.quantityHint = hint.text;
        model.quantityHintError = hint.error;
    }
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
        model.sellNote =
            "Total above the market limit of " + ExactZen(std::min(rules.maxPrice, rules.zenLimit)) + " Zen.";
        return;
    }
    const auto saleFee = Market::SaleFee(rules, static_cast<Market::Zen>(total));
    model.saleTotal = ExactZen(static_cast<Market::Zen>(total)) + " Zen";
    model.postingFee = ExactZen(*fee) + " Zen";
    model.saleFee = ExactZen(saleFee) + " Zen";
    model.proceeds = ExactZen(static_cast<Market::Zen>(total) - saleFee) + " Zen";
    if (model.listingCountFull)
        model.sellNote = "Listing limit reached (" + model.listingCount + ").";
    else if (!CharacterMachine || *fee > CharacterMachine->Gold)
        model.sellNote = "Posting fee: " + ZenShortfallText(*fee, CharacterMachine ? CharacterMachine->Gold : 0) + ".";
    model.sellReady = !model.busy && check.activeListings < rules.maxActiveListings && CharacterMachine &&
                      *fee <= CharacterMachine->Gold;
}
} // namespace UI::AuctionHouse
