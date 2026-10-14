#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace GameLogic::Buffs
{
// A server duration is independent of the legacy item's timer-family grouping.
// The caller supplies a monotonic clock so refresh and expiry are deterministic.
class EffectDuration
{
public:
    void Refresh(std::uint32_t seconds, std::uint64_t nowMs)
    {
        constexpr std::uint64_t MillisecondsPerSecond = 1000;
        m_DurationMs = seconds == std::numeric_limits<std::uint32_t>::max()
                           ? 0
                           : static_cast<std::uint64_t>(seconds) * MillisecondsPerSecond;
        m_StartedMs = nowMs;
    }

    std::uint32_t Remaining(std::uint64_t nowMs) const
    {
        const auto elapsed = nowMs >= m_StartedMs ? nowMs - m_StartedMs : 0;
        const auto remaining = elapsed < m_DurationMs ? m_DurationMs - elapsed : 0;
        return static_cast<std::uint32_t>(std::min(remaining, MaximumVisibleMilliseconds));
    }

private:
    static constexpr std::uint64_t MaximumVisibleMilliseconds = std::numeric_limits<std::uint32_t>::max() - 1;
    std::uint64_t m_StartedMs = 0;
    std::uint64_t m_DurationMs = 0;
};
} // namespace GameLogic::Buffs
