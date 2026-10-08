#include "AuctionFixture.h"
#include "GameLogic/AuctionHouse/AuctionHouse.h"
#include "GameLogic/AuctionHouse/AuctionHouseTransport.h"
#include "UI/AuctionHouse/ReviewGate.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

using namespace GameLogic::AuctionHouse;
using AuctionFixture::Id;
using AuctionFixture::Load;
using UI::AuctionHouse::ReviewGate;

namespace
{
std::vector<PacketBytes> sent;

struct Listener : IListener
{
    std::function<void()> answered;
    void OnOpened() override {}
    void OnClosed() override {}
    void OnChanged(Operation) override
    {
        if (answered)
            answered();
    }
    void OnNotice(const Notice&) override {}
};

std::uint16_t LastId()
{
    REQUIRE_FALSE(sent.empty());
    return static_cast<std::uint16_t>(sent.back()[4] | (std::uint16_t{sent.back()[5]} << 8));
}

PacketBytes ReplyWithId(std::string_view fixture, std::uint16_t id)
{
    auto packet = Load(fixture);
    const auto offset = packet[0] == 0xC2 ? 5u : 4u;
    packet[offset] = static_cast<std::uint8_t>(id);
    packet[offset + 1] = static_cast<std::uint8_t>(id >> 8);
    return packet;
}

// The window's request slot as Window.cpp drives it: one queued read, one read in flight, the open
// review with the revision it was opened at, and the gate between them.
struct WindowSlot
{
    enum class Read : std::uint8_t
    {
        None,
        Sell,
        SellQuote,
        Search
    };
    Listener listener;
    ReviewGate gate;
    Read queued = Read::None, inFlight = Read::None;
    bool reviewOpen = false, actionUnderWay = false;
    std::optional<std::uint32_t> reviewRevision;
    std::uint16_t inFlightId = 0;

    WindowSlot()
    {
        SetListener(nullptr);
        Reset();
        sent.clear();
        // OnChanged: the answer clears the read in flight, then a waiting review starts.
        listener.answered = [this]
        {
            if (inFlight != Read::None && !GetState().IsPending(Operation::SellCheck) &&
                !GetState().IsPending(Operation::Quote) && !GetState().IsPending(Operation::Search))
                inFlight = Read::None;
            if (auto start = gate.TakeWaiting(inFlight != Read::None))
                start();
        };
        SetListener(&listener);
        ReceivePacket(Load("auction-house-state-open"));
    }
    ~WindowSlot()
    {
        SetListener(nullptr);
        Reset();
    }
    // PumpRequests: the pause is checked right before a read leaves.
    void Pump()
    {
        if (queued == Read::None || inFlight != Read::None ||
            !ReviewGate::MayDispatchRead(reviewOpen, actionUnderWay))
            return;
        const auto read = std::exchange(queued, Read::None);
        const bool ok = read == Read::Sell        ? CheckSell(12)
                        : read == Read::SellQuote ? RequestQuote(7181, 0, 1)
                                                  : RequestSearch({}, 0);
        REQUIRE(ok);
        inFlight = read;
        inFlightId = LastId();
    }
    void OpenReview()
    {
        reviewOpen = true;
        reviewRevision = GetState().Revision();
    }
    void AskForReview()
    {
        gate.Start(inFlight != Read::None, [this] { OpenReview(); });
    }
    // Confirm and SendAction: an unchanged market revision.
    bool Confirm() const
    {
        return reviewOpen && reviewRevision == GetState().Revision();
    }
    void DismissReview()
    {
        gate.Cancel();
        reviewOpen = false;
    }
};
} // namespace

namespace GameLogic::AuctionHouse::Transport
{
bool Send(std::span<const std::uint8_t> bytes)
{
    sent.emplace_back(bytes.begin(), bytes.end());
    return true;
}
void CloseDialog() {}
} // namespace GameLogic::AuctionHouse::Transport

TEST_CASE("Auction a review asked for while a Sell read is in flight opens after its answer [auction][review-gate]")
{
    WindowSlot window;
    window.queued = WindowSlot::Read::Sell;
    window.Pump();
    REQUIRE(window.inFlight == WindowSlot::Read::Sell);

    window.AskForReview();
    CHECK_FALSE(window.reviewOpen);
    CHECK(window.gate.Waiting());

    // The 5 s poll follows a sell check with the market depth; it waits behind the review.
    ReceivePacket(ReplyWithId("auction-sell-check-result", window.inFlightId));
    window.queued = WindowSlot::Read::SellQuote;
    CHECK(window.inFlight == WindowSlot::Read::None);
    REQUIRE(window.reviewOpen);
    CHECK_FALSE(window.gate.Waiting());
    const auto before = sent.size();
    window.Pump();
    CHECK(sent.size() == before);
    CHECK(window.queued == WindowSlot::Read::SellQuote);

    // Nothing answered while the review was open, so Confirm publishes the reviewed listing.
    CHECK(window.Confirm());
    REQUIRE(CreateListing(12, 18, 1000000, 24, 181000));
    ReceivePacket(ReplyWithId("auction-create-listing-result", LastId()));
    CHECK(GetState().LastOutcome(Operation::CreateListing)->result == Result::Success);
}

TEST_CASE("Auction a read queued before a review opened waits until the review closes [auction][review-gate]")
{
    WindowSlot window;
    window.queued = WindowSlot::Read::Sell;
    window.AskForReview();
    REQUIRE(window.reviewOpen);
    const auto before = sent.size();
    window.Pump();
    CHECK(sent.size() == before);
    CHECK(window.Confirm());

    // An action under way pauses reads too.
    window.actionUnderWay = true;
    window.DismissReview();
    window.Pump();
    CHECK(sent.size() == before);

    window.actionUnderWay = false;
    window.Pump();
    CHECK(sent.size() == before + 1);
    CHECK(window.inFlight == WindowSlot::Read::Sell);
}

TEST_CASE("Auction without the gate an answer during the review makes Confirm refuse it [auction][review-gate]")
{
    // What CR-02 described: the review opens with the sell check still in flight.
    WindowSlot window;
    window.queued = WindowSlot::Read::Sell;
    window.Pump();
    window.OpenReview();
    ReceivePacket(ReplyWithId("auction-sell-check-result", window.inFlightId));
    CHECK_FALSE(window.Confirm());
}

TEST_CASE("Auction a waiting review is dropped when the player moves on [auction][review-gate]")
{
    WindowSlot window;
    window.queued = WindowSlot::Read::Sell;
    window.Pump();
    window.AskForReview();
    REQUIRE(window.gate.Waiting());
    window.DismissReview();
    ReceivePacket(ReplyWithId("auction-sell-check-result", window.inFlightId));
    CHECK_FALSE(window.reviewOpen);

    // A later request replaces an earlier waiting one; only the latest starts.
    ReviewGate gate;
    int first = 0, second = 0;
    CHECK_FALSE(gate.Start(true, [&first] { ++first; }));
    CHECK_FALSE(gate.Start(true, [&second] { ++second; }));
    CHECK_FALSE(gate.TakeWaiting(true));
    auto start = gate.TakeWaiting(false);
    REQUIRE(start);
    start();
    CHECK(first == 0);
    CHECK(second == 1);
    CHECK_FALSE(gate.TakeWaiting(false));
    CHECK(gate.Start(false, [&first] { ++first; }));
    CHECK(first == 1);
}

TEST_CASE("Auction Compare market: a notice during a purchase review keeps it and Confirm buys [auction][review-gate]")
{
    // Compare market runs a search; the player selects a listing and reviews it, then a sale notice arrives.
    WindowSlot window;
    window.queued = WindowSlot::Read::Search;
    window.Pump();
    ReceivePacket(ReplyWithId("auction-search-result", window.inFlightId));
    REQUIRE(GetState().Search().rows.size() == 2);
    window.AskForReview();
    REQUIRE(window.reviewOpen);
    ReceivePacket(Load("auction-notice-sold"));
    CHECK(window.reviewOpen);
    CHECK(window.Confirm());
    const auto& listing = GetState().Search().rows[0];
    CHECK(BuyListing(listing.id, listing.price));
}
