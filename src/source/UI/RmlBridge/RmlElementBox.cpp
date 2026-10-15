#include "stdafx.h"
#include "UI/RmlBridge/RmlElementBox.h"

#include <cmath>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementUtilities.h>

namespace
{
// Full unclipped two-corner extent, not an AABB: keep transformed top-left semantics for rotated roots.
bool LayoutToScreen(Rml::Element& element, Rml::Vector2f layoutOffset, Rml::Vector2f layoutSize,
    Rml::Vector2f& offset, Rml::Vector2f& size)
{
    auto topLeft = layoutOffset, bottomRight = layoutOffset + layoutSize;
    if (!Rml::ElementUtilities::TransformPoint(&element, topLeft) ||
        !Rml::ElementUtilities::TransformPoint(&element, bottomRight))
        return false;
    const auto extent = bottomRight - topLeft;
    if (!std::isfinite(extent.x) || !std::isfinite(extent.y))
        return false;
    offset = topLeft;
    size = extent;
    return true;
}
} // namespace

bool UI::RmlBridge::DrawnBox(Rml::Element& element, Rml::BoxArea area, Rml::Vector2f& offset, Rml::Vector2f& size)
{
    Rml::Vector2f preparedOffset, preparedSize;
    if (!LayoutToScreen(element, element.GetAbsoluteOffset(area), element.GetBox().GetSize(area), preparedOffset,
                        preparedSize) ||
        preparedSize.x <= 0.f || preparedSize.y <= 0.f)
        return false;
    offset = preparedOffset;
    size = preparedSize;
    return true;
}

bool UI::RmlBridge::DrawnContentBox(Rml::Element& element, Rml::Vector2f& offset, Rml::Vector2f& size)
{
    return DrawnBox(element, Rml::BoxArea::Content, offset, size);
}

bool UI::RmlBridge::DrawnTopLeft(Rml::Element& element, Rml::Vector2f& point)
{
    Rml::Vector2f size;
    return LayoutToScreen(element, element.GetAbsoluteOffset(Rml::BoxArea::Border), Rml::Vector2f(0.f, 0.f), point, size);
}

float UI::RmlBridge::DrawnScale(Rml::Element& element)
{
    const float layoutWidth = element.GetBox().GetSize(Rml::BoxArea::Border).x;
    Rml::Vector2f offset;
    Rml::Vector2f size;
    if (layoutWidth <= 0.f || !DrawnBox(element, Rml::BoxArea::Border, offset, size))
        return 1.f;
    return size.x / layoutWidth;
}
