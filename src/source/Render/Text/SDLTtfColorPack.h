#pragma once

#include <cstdint>

namespace mu::sdlttf
{

[[nodiscard]] constexpr std::uint32_t PackColorDWORD(std::uint8_t red, std::uint8_t green, std::uint8_t blue,
                                                     std::uint8_t alpha) noexcept
{
    return (static_cast<std::uint32_t>(alpha) << 24) | (static_cast<std::uint32_t>(blue) << 16) |
           (static_cast<std::uint32_t>(green) << 8) | static_cast<std::uint32_t>(red);
}

struct UnpackedColor
{
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
    std::uint8_t alpha;
};

// Inverse of PackColorDWORD() for colors handed to the text renderer as one DWORD
// (SetTextColor(DWORD), getGoldColor()).
[[nodiscard]] constexpr UnpackedColor UnpackColorDWORD(std::uint32_t packed) noexcept
{
    constexpr std::uint32_t kByteMask = 0xFFu;
    return {static_cast<std::uint8_t>(packed & kByteMask), static_cast<std::uint8_t>((packed >> 8) & kByteMask),
            static_cast<std::uint8_t>((packed >> 16) & kByteMask),
            static_cast<std::uint8_t>((packed >> 24) & kByteMask)};
}

} // namespace mu::sdlttf
