#pragma once

#include <cstdint>
#include <string>

namespace UI::RmlBridge
{
// CSS rgba() for a color packed for the native text renderer (mu::sdlttf::PackColorDWORD()
// byte order), e.g. getGoldColor()'s result, so a legacy-theme binding shows the native color.
[[nodiscard]] std::string PackedTextColorToCss(std::uint32_t packedTextColor);
} // namespace UI::RmlBridge
