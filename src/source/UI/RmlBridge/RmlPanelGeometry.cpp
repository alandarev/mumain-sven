#include "stdafx.h"
#include "Core/Utilities/FrameProfiler.h"
#include "RmlPanelGeometry.h"
#include "RmlDraggable.h"

#include "UI/Scaling/UITransform.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Types.h>

#include <string>
#include <charconv>
#include <cmath>
#include <limits>

bool UI::RmlBridge::ApplyLayoutScale(Rml::Element* root, float effectiveScale, bool publishScale)
{
    if (root == nullptr)
        return false;
    const bool validScale = std::isfinite(effectiveScale) && effectiveScale > 0.f;
    const auto declaration = root->GetAttribute<Rml::String>("data-compact-scale", "");
    float threshold = 0.f;
    const auto parsed = std::from_chars(declaration.data(), declaration.data() + declaration.size(), threshold);
    const bool optedIn = parsed.ec == std::errc{} && parsed.ptr == declaration.data() + declaration.size() &&
                         std::isfinite(threshold) && threshold > 0.f;
    // Attribute presence is cleanup ownership only; its numeric value never supplies layout state.
    const bool hadGeneratedScale = root->HasAttribute("data-layout-scale");
    std::string scaleText;
    if (validScale)
    {
        // Enough for every finite float at round-trip precision, including sign and exponent.
        char buffer[32];
        const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), effectiveScale,
                                             std::chars_format::general, std::numeric_limits<float>::max_digits10);
        scaleText.assign(buffer, converted.ptr);
    }
    if (validScale && (publishScale || optedIn))
    {
        const auto* property = root->GetLocalProperty("--root-scale");
        if (property == nullptr || property->ToString() != scaleText)
            root->SetProperty("--root-scale", scaleText);
    }
    else if (publishScale || hadGeneratedScale)
        root->RemoveProperty("--root-scale");

    root->SetClass("compact-scale", validScale && optedIn && effectiveScale < threshold);
    if (validScale && optedIn)
    {
        if (root->GetAttribute<Rml::String>("data-layout-scale", "") != scaleText)
            root->SetAttribute("data-layout-scale", scaleText);
    }
    else
        root->RemoveAttribute("data-layout-scale");
    return validScale && optedIn;
}

bool UI::RmlBridge::FillPlacementSize::Set(float newWidth, float newHeight)
{
    if (width == newWidth && height == newHeight)
        return false;
    width = newWidth;
    height = newHeight;
    return true;
}

void UI::RmlBridge::FillPlacementSize::Apply(Rml::ElementDocument* doc, const char* panelId) const
{
    Rml::Element* panel = doc != nullptr ? doc->GetElementById(panelId) : nullptr;
    if (panel == nullptr)
        return;

    const bool fill = width > 0.f && height > 0.f;
    panel->SetClass("fill-placement", fill);
    if (fill)
    {
        panel->SetProperty(Rml::PropertyId::Width, Rml::Property(width, Rml::Unit::PX));
        panel->SetProperty(Rml::PropertyId::Height, Rml::Property(height, Rml::Unit::PX));
    }
    else
    {
        panel->RemoveProperty(Rml::PropertyId::Width);
        panel->RemoveProperty(Rml::PropertyId::Height);
    }
    {
        FRAME_PROFILE(UILayout);
        doc->UpdateDocument();
    }
}

void UI::RmlBridge::FillPlacementSize::Sync(Rml::ElementDocument* doc, const char* panelId) const
{
    if (width <= 0.f || height <= 0.f || doc == nullptr)
        return;
    Rml::Element* panel = doc->GetElementById(panelId);
    if (panel != nullptr && !panel->IsClassSet("fill-placement"))
        Apply(doc, panelId);
}

bool UI::RmlBridge::SlotPlacement::Set(float newLeft, float newTop, float newScale)
{
    if (left == newLeft && top == newTop && scale == newScale)
        return false;
    left = newLeft;
    top = newTop;
    scale = newScale;
    return true;
}

void UI::RmlBridge::SlotPlacement::Apply(Rml::ElementDocument* doc, const char* panelId) const
{
    Rml::Element* panel = doc != nullptr ? doc->GetElementById(panelId) : nullptr;
    if (panel == nullptr)
        return;

    // A new place takes over from where the player dragged it.
    ResetDrag(doc, panelId);
    const bool placed = std::isfinite(scale) && scale > 0.f;
    panel->SetClass("slot-placed", placed);
    if (placed)
    {
        panel->SetProperty("--slot-left", std::to_string(left) + "px");
        panel->SetProperty("--slot-top", std::to_string(top) + "px");
    }
    else
    {
        panel->RemoveProperty("--slot-left");
        panel->RemoveProperty("--slot-top");
    }
    ApplyLayoutScale(panel, scale);
}

void UI::RmlBridge::SlotPlacement::Sync(Rml::ElementDocument* doc, const char* panelId) const
{
    if (scale <= 0.f || doc == nullptr)
        return;
    Rml::Element* panel = doc->GetElementById(panelId);
    if (panel != nullptr && !panel->IsClassSet("slot-placed"))
        Apply(doc, panelId);
    else
        ApplyLayoutScale(panel, scale);
}

void UI::RmlBridge::SlotPlacement::ResetDrag(Rml::ElementDocument* doc, const char* panelId)
{
    Rml::Element* panel = doc != nullptr ? doc->GetElementById(panelId) : nullptr;
    if (panel != nullptr && panel->IsClassSet("dragged"))
        ResetDraggedPosition(panel);
}
