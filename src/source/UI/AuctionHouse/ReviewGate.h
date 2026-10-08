#pragma once

#include <functional>

namespace UI::AuctionHouse
{
// The one rule between the window's reads and its reviews (D7, CR-02). Every answer of the market
// changes its revision, and Confirm refuses a review whose revision changed. So no read leaves while
// a review is open or an action is queued or under way (it stays queued until then), and a review or
// a collection never starts while a read is in flight: one asked for meanwhile starts once the
// answer is in, from the then current state.
class ReviewGate
{
public:
    // Checked right before a queued read is sent.
    static bool MayDispatchRead(bool reviewOpen, bool actionUnderWay)
    {
        return !reviewOpen && !actionUnderWay;
    }
    // Starts `start` now and returns true, or keeps it (replacing an earlier one) until the read in
    // flight is answered.
    bool Start(bool readInFlight, std::function<void()> start);
    // After an answer: hands out the waiting start once no read is in flight any more.
    std::function<void()> TakeWaiting(bool readInFlight);
    // The player did something else, the read timed out, or the window closed.
    void Cancel()
    {
        m_Waiting = nullptr;
    }
    bool Waiting() const
    {
        return static_cast<bool>(m_Waiting);
    }

private:
    std::function<void()> m_Waiting;
};
} // namespace UI::AuctionHouse
