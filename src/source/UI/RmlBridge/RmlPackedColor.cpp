#include "UI/RmlBridge/RmlPackedColor.h"

#include <cstdio>

#include "Render/Text/SDLTtfColorPack.h"

std::string UI::RmlBridge::PackedTextColorToCss(std::uint32_t packedTextColor)
{
    const mu::sdlttf::UnpackedColor color = mu::sdlttf::UnpackColorDWORD(packedTextColor);
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "rgba(%u,%u,%u,%u)", static_cast<unsigned>(color.red),
                  static_cast<unsigned>(color.green), static_cast<unsigned>(color.blue),
                  static_cast<unsigned>(color.alpha));
    return buffer;
}
