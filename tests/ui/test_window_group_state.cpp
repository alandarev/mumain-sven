#include <doctest.h>

#include "UI/Placement/WindowGroupState.h"

using UI::Placement::WindowGroupState;

TEST_CASE("A joining companion retains the player's Inventory anchor when it fits [ui][window-groups]")
{
    WindowGroupState state;
    const WindowGroupState::Bounds safe{0.f, 60.f, 1920.f, 920.f};
    state.SetPreferred(WindowGroupState::Normalize({1710.f, 140.f}, safe));
    const auto shift = state.Translation(safe, {450.f, 60.f, 1470.f, 740.f}, {1920.f, 60.f});
    CHECK(shift.x == doctest::Approx(-210.f));
    CHECK(shift.y == doctest::Approx(80.f));
    const auto close =
        state.Translation(safe, {1540.f, 60.f, 380.f, 450.f}, {1920.f, 60.f}, WindowGroupState::Point{1710.f, 140.f});
    CHECK(close.x == doctest::Approx(-210.f));
    CHECK(close.y == doctest::Approx(80.f));
}

TEST_CASE("Temporary clamping does not replace a preferred anchor [ui][window-groups]")
{
    WindowGroupState state;
    const WindowGroupState::Bounds large{0.f, 60.f, 1920.f, 920.f};
    const auto preferred = WindowGroupState::Normalize({1400.f, 180.f}, large);
    state.SetPreferred(preferred);
    const WindowGroupState::Bounds small{0.f, 40.f, 1024.f, 650.f};
    const auto clamp = state.Translation(small, {0.f, 40.f, 1024.f, 650.f}, {1024.f, 40.f});
    CHECK(clamp.x == doctest::Approx(0.f));
    CHECK(clamp.y == doctest::Approx(0.f));
    REQUIRE(state.Preferred());
    CHECK(state.Preferred()->x == preferred.x);
    CHECK(state.Preferred()->y == preferred.y);
    const auto restored = state.Translation(large, {920.f, 60.f, 1000.f, 740.f}, {1920.f, 60.f});
    CHECK(restored.x == doctest::Approx(-520.f));
    CHECK(restored.y == doctest::Approx(120.f));
}

TEST_CASE("A complete group fits proportionally while preserving its theme gap [ui][window-groups]")
{
    const float gap = 16.f;
    const WindowGroupState::Bounds available{0.f, 40.f, 1024.f, 650.f};
    const WindowGroupState::Bounds requested{0.f, 40.f, 1456.f, 740.f};
    const float factor = WindowGroupState::FitFactor(available, requested, gap);
    CHECK(factor == doctest::Approx((1024.f - gap) / (1456.f - gap)));
    CHECK((requested.width - gap) * factor + gap <= available.width + .001f);
    CHECK(requested.height * factor <= available.height);
    CHECK(WindowGroupState::FitFactor(available, {0.f, 40.f, 600.f, 450.f}, gap) == 1.f);
}
