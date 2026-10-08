#include "UI/AuctionHouse/ReviewGate.h"

#include <utility>

namespace UI::AuctionHouse
{
bool ReviewGate::Start(bool readInFlight, std::function<void()> start)
{
    if (!start)
        return false;
    if (readInFlight)
    {
        m_Waiting = std::move(start);
        return false;
    }
    m_Waiting = nullptr;
    start();
    return true;
}

std::function<void()> ReviewGate::TakeWaiting(bool readInFlight)
{
    if (readInFlight)
        return nullptr;
    return std::exchange(m_Waiting, nullptr);
}
} // namespace UI::AuctionHouse
