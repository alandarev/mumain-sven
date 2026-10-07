#include "BuffPresentation.h"

#include <limits>

namespace UI::HUD
{
namespace
{
constexpr std::uint32_t MillisecondsPerSecond = 1000;
constexpr std::uint32_t SecondsPerMinute = 60;
constexpr std::uint32_t SecondsPerHour = SecondsPerMinute * SecondsPerMinute;
constexpr std::uint32_t ExpiringMilliseconds = 10 * MillisecondsPerSecond;
bool HasDuration(std::uint32_t value)
{
    return value != 0 && value != std::numeric_limits<std::uint32_t>::max();
}
} // namespace

std::string CompactBuffDuration(std::uint32_t remainingMilliseconds)
{
    if (!HasDuration(remainingMilliseconds))
        return {};
    const auto seconds =
        (static_cast<std::uint64_t>(remainingMilliseconds) + MillisecondsPerSecond - 1) / MillisecondsPerSecond;
    if (seconds < SecondsPerMinute)
        return std::to_string(seconds) + "s";
    if (seconds < SecondsPerHour)
        return std::to_string((seconds + SecondsPerMinute - 1) / SecondsPerMinute) + "m";
    return std::to_string((seconds + SecondsPerHour - 1) / SecondsPerHour) + "h";
}

bool BuffExpiresSoon(std::uint32_t remainingMilliseconds)
{
    return HasDuration(remainingMilliseconds) && remainingMilliseconds <= ExpiringMilliseconds;
}
} // namespace UI::HUD
