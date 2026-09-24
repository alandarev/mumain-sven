#include "doctest.h"

#include "Render/Text/SDLTtfColorPack.h"
#include "UI/RmlBridge/RmlPackedColor.h"

TEST_CASE("SDL_ttf colors use the renderer ABGR byte order")
{
    CHECK(mu::sdlttf::PackColorDWORD(0x12, 0x34, 0x56, 0x78) == 0x78563412u);
    CHECK(mu::sdlttf::PackColorDWORD(0xFF, 0xFF, 0xFF, 0xFF) == 0xFFFFFFFFu);
    CHECK(mu::sdlttf::PackColorDWORD(0, 0, 0, 0) == 0u);
}

TEST_CASE("SDL_ttf packed colors unpack in the renderer byte order")
{
    const auto color = mu::sdlttf::UnpackColorDWORD(mu::sdlttf::PackColorDWORD(0x12, 0x34, 0x56, 0x78));
    CHECK(color.red == 0x12);
    CHECK(color.green == 0x34);
    CHECK(color.blue == 0x56);
    CHECK(color.alpha == 0x78);
}

TEST_CASE("Packed text colors become the native color in CSS")
{
    // getGoldColor()'s 10,000,000+ Zen color, (255 << 24) + (0 << 16) + (0 << 8) + 255, is red on
    // the native text renderer.
    CHECK(UI::RmlBridge::PackedTextColorToCss((255u << 24) + 255u) == "rgba(255,0,0,255)");
    CHECK(UI::RmlBridge::PackedTextColorToCss(mu::sdlttf::PackColorDWORD(24, 201, 0, 255)) == "rgba(24,201,0,255)");
}
