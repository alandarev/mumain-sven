#pragma once
#include <chrono>

namespace UI::AuctionHouse
{
class RequestClock
{
public:
    using Time = std::chrono::steady_clock::time_point;
    bool Ready(Time now) const
    {
        return now >= m_Next;
    }
    void Sent(Time now)
    {
        m_Next = now + std::chrono::milliseconds(260);
    }
    void Busy(Time now)
    {
        m_Next = now + std::chrono::seconds(1);
    }
    void Reset()
    {
        m_Next = {};
    }

private:
    Time m_Next{};
};
} // namespace UI::AuctionHouse
