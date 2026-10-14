#pragma once

#include <cstdint>
#include <string>

namespace UI::HUD
{
std::string CompactBuffDuration(std::uint32_t remainingMilliseconds);
bool BuffExpiresSoon(std::uint32_t remainingMilliseconds);
} // namespace UI::HUD
