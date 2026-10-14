#pragma once

#include <algorithm>
#include <cstdint>

namespace UI::NPCs
{
// Owns request ordering only. Stock, balances and item updates remain engine/server facts.
class PurchaseBatch
{
public:
    bool Begin(int slot, std::uint32_t key, int offers)
    {
        if (Active() || slot < 0 || slot >= 120 || key == 0 || offers < 1 || offers > 100)
            return false;
        m_Slot = slot;
        m_Key = key;
        m_Requested = offers;
        m_Confirmed = 0;
        return true;
    }
    bool Send()
    {
        if (!Active() || m_InFlight)
            return false;
        m_InFlight = true;
        return true;
    }
    bool Answer(bool success)
    {
        if (!m_InFlight)
            return false;
        m_InFlight = false;
        if (success)
            ++m_Confirmed;
        else
            m_Requested = m_Confirmed;
        return true;
    }
    void Stop()
    {
        m_Requested = m_Confirmed + (m_InFlight ? 1 : 0);
    }
    bool Active() const
    {
        return m_InFlight || m_Confirmed < m_Requested;
    }
    bool InFlight() const
    {
        return m_InFlight;
    }
    int Confirmed() const
    {
        return m_Confirmed;
    }
    int Requested() const
    {
        return m_Requested;
    }
    int Slot() const
    {
        return m_Slot;
    }
    std::uint32_t Key() const
    {
        return m_Key;
    }

private:
    int m_Slot = -1;
    std::uint32_t m_Key = 0;
    int m_Requested = 0;
    int m_Confirmed = 0;
    bool m_InFlight = false;
};
} // namespace UI::NPCs
