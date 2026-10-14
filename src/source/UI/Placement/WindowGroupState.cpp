#include "UI/Placement/WindowGroupState.h"

#include <algorithm>

namespace UI::Placement
{
float WindowGroupState::FitFactor(const Bounds& available, const Bounds& requested, float fixedWidth)
{
    float fit = 1.f;
    const float scalableWidth = requested.width - fixedWidth;
    if (scalableWidth > 0.f)
        fit = std::min(fit, std::max(0.f, available.width - fixedWidth) / scalableWidth);
    if (requested.height > 0.f)
        fit = std::min(fit, available.height / requested.height);
    return fit;
}

WindowGroupState::Point WindowGroupState::Normalize(Point anchor, const Bounds& available)
{
    return {available.width > 0.f ? (available.left + available.width - anchor.x) / available.width : 0.f,
            available.height > 0.f ? (anchor.y - available.top) / available.height : 0.f};
}

void WindowGroupState::SetPreferred(Point normalized)
{
    m_Preferred = normalized;
}

std::optional<WindowGroupState::Point> WindowGroupState::Preferred() const
{
    return m_Preferred;
}

WindowGroupState::Point WindowGroupState::Translation(const Bounds& available, const Bounds& group, Point anchor,
                                                      std::optional<Point> retained) const
{
    Point target = anchor;
    if (retained)
        target = *retained;
    else if (m_Preferred)
        target = {available.left + available.width - m_Preferred->x * available.width,
                  available.top + m_Preferred->y * available.height};
    const float maxLeft = available.left + std::max(0.f, available.width - group.width);
    const float maxTop = available.top + std::max(0.f, available.height - group.height);
    return {std::clamp(group.left + target.x - anchor.x, available.left, maxLeft) - group.left,
            std::clamp(group.top + target.y - anchor.y, available.top, maxTop) - group.top};
}
} // namespace UI::Placement
