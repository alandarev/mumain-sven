#pragma once

#include <optional>

namespace UI::Placement
{
// A preferred anchor survives temporary clamping and resolution changes. Geometry comes from
// the theme workspace; the anchor is its declared pane's right/top edge.
class WindowGroupState
{
public:
    struct Point
    {
        float x = 0.f;
        float y = 0.f;
    };
    struct Bounds
    {
        float left = 0.f;
        float top = 0.f;
        float width = 0.f;
        float height = 0.f;
    };

    static float FitFactor(const Bounds& available, const Bounds& requested, float fixedWidth);
    static Point Normalize(Point anchor, const Bounds& available);
    void SetPreferred(Point normalized);
    std::optional<Point> Preferred() const;
    Point Translation(const Bounds& available, const Bounds& group, Point anchor,
                      std::optional<Point> retained = std::nullopt) const;

    bool active = false;
    float fit = 1.f;
    std::optional<Point> lastAnchor;

private:
    std::optional<Point> m_Preferred;
};
} // namespace UI::Placement
