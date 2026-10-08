#include "GameLogic/AuctionHouse/AuctionHouseController.h"
#include "GameLogic/AuctionHouse/AuctionHouseTransport.h"

#include <algorithm>

namespace GameLogic::AuctionHouse
{
bool State::IsPending(Operation op) const
{
    return op < Operation::Count && requests[static_cast<std::size_t>(op)].pending;
}

std::optional<Outcome> State::LastOutcome(Operation op) const
{
    if (op >= Operation::Count)
        return std::nullopt;
    return requests[static_cast<std::size_t>(op)].outcome;
}
} // namespace GameLogic::AuctionHouse

namespace GameLogic::AuctionHouse::Detail
{
namespace
{
constexpr std::array Mutations{Operation::BuyListing, Operation::BuyGoods, Operation::CreateListing,
                               Operation::CancelListing, Operation::Claim};
constexpr std::size_t MaximumRememberedNotices = 32;

bool IsMutation(Operation op)
{
    return std::find(Mutations.begin(), Mutations.end(), op) != Mutations.end();
}
} // namespace

std::uint16_t Controller::NextId()
{
    if (++nextId == 0)
        ++nextId;
    return nextId;
}

bool Controller::Send(Operation op, PacketBytes packet)
{
    if (!state.open || op >= Operation::Count || packet.size() < 6)
        return false;
    if (IsMutation(op) &&
        std::any_of(Mutations.begin(), Mutations.end(), [this](auto mutation) { return state.IsPending(mutation); }))
        return false;
    auto& request = state.requests[static_cast<std::size_t>(op)];
    const auto previous = request;
    request.id = static_cast<std::uint16_t>(packet[4] | (std::uint16_t{packet[5]} << 8));
    request.pending = true;
    request.outcome.reset();
    if (!Transport::Send(packet))
    {
        request = previous;
        return false;
    }
    if (op == Operation::Quote || op == Operation::BuyGoods)
        state.quote.reset();
    if (op == Operation::Quote || op == Operation::BuyGoods)
        awaitingPurchaseQuote = false;
    if (op == Operation::BuyGoods)
        state.requests[static_cast<std::size_t>(Operation::Quote)].pending = false;
    if (op == Operation::SellCheck)
        state.sellCheck.reset();
    Changed(op);
    return true;
}

void Controller::Changed(Operation op)
{
    ++state.revision;
    if (listener)
        listener->OnChanged(op);
}

bool Controller::Accept(Operation op, const Reply& reply, bool replacementQuote)
{
    if (!state.open)
        return false;
    auto& request = state.requests[static_cast<std::size_t>(op)];
    if (!replacementQuote && (!request.pending || request.id != reply.requestId))
        return false;
    if (reply.result == Result::NotOpen)
    {
        Reset();
        return false;
    }
    if (!replacementQuote)
        request.pending = false;
    Outcome outcome;
    static_cast<Reply&>(outcome) = reply;
    outcome.operation = op;
    request.outcome = outcome;
    return true;
}

void Controller::Apply(RulesUpdate value)
{
    if (!value.open)
    {
        Reset();
        return;
    }
    state.open = true;
    state.rules = std::move(value.rules);
    ++state.revision;
    if (listener)
        listener->OnOpened();
}

void Controller::Apply(SearchPage value)
{
    if (!Accept(Operation::Search, value))
        return;
    value.receivedAt = std::chrono::steady_clock::now();
    state.search = std::move(value);
    Changed(Operation::Search);
}

void Controller::Apply(GoodsPage value)
{
    if (!Accept(Operation::GoodsList, value))
        return;
    value.receivedAt = std::chrono::steady_clock::now();
    state.goods = std::move(value);
    Changed(Operation::GoodsList);
}

void Controller::Apply(Quote value)
{
    const bool replacement = awaitingPurchaseQuote && value.requestId == purchaseQuoteId;
    if (!Accept(Operation::Quote, value, replacement))
        return;
    if (replacement)
        awaitingPurchaseQuote = false;
    value.receivedAt = std::chrono::steady_clock::now();
    state.quote = std::move(value);
    Changed(Operation::Quote);
}

void Controller::Apply(SellCheck value)
{
    if (!Accept(Operation::SellCheck, value))
        return;
    state.rules.activeListings = value.activeListings;
    state.sellCheck = value;
    Changed(Operation::SellCheck);
}

void Controller::Apply(OwnListingsPage value)
{
    if (!Accept(Operation::OwnListings, value))
        return;
    value.receivedAt = std::chrono::steady_clock::now();
    state.ownListings = std::move(value);
    if (state.ownListings.result == Result::Success)
        state.rules.activeListings = static_cast<std::uint8_t>(state.ownListings.rows.size());
    Changed(Operation::OwnListings);
}

void Controller::Apply(CollectionPage value)
{
    if (!Accept(Operation::Collection, value))
        return;
    value.receivedAt = std::chrono::steady_clock::now();
    state.collection = std::move(value);
    if (state.collection.result == Result::Success)
        state.rules.pendingCollection = state.collection.totalCount;
    Changed(Operation::Collection);
}

void Controller::Apply(Outcome value)
{
    if (!Accept(value.operation, value))
        return;
    state.requests[static_cast<std::size_t>(value.operation)].outcome = value;
    if (value.operation == Operation::BuyGoods)
    {
        awaitingPurchaseQuote = value.result == Result::QuoteChanged || value.result == Result::InsufficientStock;
        purchaseQuoteId = value.requestId;
        state.quote.reset();
    }
    if (value.operation == Operation::Claim && (value.result == Result::Success || value.result == Result::Partial))
        state.rules.pendingCollection = value.remainingEntries;
    Changed(value.operation);
}

void Controller::Apply(Notice value)
{
    state.rules.pendingCollection = value.pendingCollection;
    if (state.notices.size() == MaximumRememberedNotices)
        state.notices.erase(state.notices.begin());
    state.notices.push_back(value);
    // A sale or expiry changes nothing an open review depends on, so it keeps the revision (D19).
    if (listener)
        listener->OnNotice(value);
}

void Controller::Receive(std::span<const std::uint8_t> packet)
{
    auto decoded = ParseServerPacket(packet);
    if (decoded)
        std::visit([this](auto&& value) { Apply(std::forward<decltype(value)>(value)); }, std::move(*decoded));
}

void Controller::Reset()
{
    const bool wasOpen = state.open;
    const auto revision = state.revision + 1;
    state = {};
    state.revision = revision;
    awaitingPurchaseQuote = false;
    purchaseQuoteId = 0;
    if (wasOpen && listener)
        listener->OnClosed();
}
} // namespace GameLogic::AuctionHouse::Detail
