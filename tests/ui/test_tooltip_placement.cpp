#include "doctest.h"
#include "UI/RmlBridge/RmlTooltipPlacement.h"

namespace Placement = UI::RmlBridge::TooltipPlacement;

TEST_CASE("a button's hint is centred 3 units right of the button, 2 units off its edges")
{
    // A 36 x 29 button at (100, 200), 2 pixels per unit.
    const Placement::ButtonHintAnchor anchor = Placement::ForButton(100.f, 200.f, 72.f, 58.f, 2.f);
    CHECK(anchor.x == doctest::Approx(100.f + 36.f + 6.f));
    CHECK(anchor.belowY == doctest::Approx(200.f + 58.f + 4.f));
    CHECK(anchor.aboveY == doctest::Approx(200.f - 4.f));
}

TEST_CASE("a tooltip that fits stays on its own side of the anchor")
{
    const float flip = 40.f;
    CHECK(Placement::Top(100.f, false, &flip, 20.f, 600.f) == doctest::Approx(100.f));
    CHECK(Placement::Top(100.f, true, &flip, 20.f, 600.f) == doctest::Approx(80.f));
}

TEST_CASE("a tooltip that leaves the viewport takes the other side of the button")
{
    // Below a button at the bottom of the screen: grows up from the button's top instead.
    const float buttonTop = 560.f;
    CHECK(Placement::Top(595.f, false, &buttonTop, 20.f, 600.f) == doctest::Approx(540.f));
    // Above a button at the top of the screen: grows down from the button's bottom instead.
    const float buttonBottom = 30.f;
    CHECK(Placement::Top(5.f, true, &buttonBottom, 20.f, 600.f) == doctest::Approx(30.f));
}

TEST_CASE("a tooltip with no room on either side, or no flip anchor, is shifted on screen")
{
    CHECK(Placement::Top(595.f, false, nullptr, 20.f, 600.f) == doctest::Approx(580.f));
    const float flip = 10.f;
    CHECK(Placement::Top(595.f, false, &flip, 20.f, 600.f) == doctest::Approx(580.f));
    CHECK(Placement::Top(0.f, false, nullptr, 700.f, 600.f) == doctest::Approx(0.f));
}

TEST_CASE("an item tooltip follows a lower cursor while the viewport has room below it")
{
    // Below the cursor's bottom (anchor), flipping above its top (600) only when needed.
    const float cursorTop = 600.f;
    CHECK(Placement::Top(620.f, false, &cursorTop, 300.f, 1080.f) == doctest::Approx(620.f));
    const float lowerCursorTop = 630.f;
    CHECK(Placement::Top(650.f, false, &lowerCursorTop, 300.f, 1080.f) == doctest::Approx(650.f));
}

TEST_CASE("a bottom-edge item tooltip flips above the cursor rather than across it")
{
    const float cursorTop = 940.f;
    const float top = Placement::Top(960.f, false, &cursorTop, 300.f, 1080.f);
    CHECK(top + 300.f == doctest::Approx(cursorTop));
}

TEST_CASE("an above tooltip keeps its direction until the top edge makes it flip at its anchor")
{
    float anchor = 800.f;
    CHECK(Placement::Top(anchor, true, &anchor, 300.f, 1080.f) + 300.f == doctest::Approx(800.f));
    anchor = 40.f;
    CHECK(Placement::Top(anchor, true, &anchor, 300.f, 1080.f) == doctest::Approx(40.f));
}

TEST_CASE("a tooltip stays inside each viewport when neither side has room")
{
    const float viewports[][2]{{1024.f, 768.f}, {1920.f, 1080.f}, {3840.f, 2160.f}};
    for (const auto& viewport : viewports)
    {
        const float anchor = viewport[1] - 10.f;
        const float top = Placement::Top(anchor, false, &anchor, viewport[1] + 100.f, viewport[1]);
        CHECK(top == doctest::Approx(0.f));
        const float fits = Placement::Top(anchor, false, &anchor, 200.f, viewport[1]);
        CHECK(fits >= 0.f);
        CHECK(fits + 200.f <= viewport[1]);
    }
}
