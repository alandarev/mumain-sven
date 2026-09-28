#pragma once

#include <RmlUi/Core/Types.h>

namespace mu::ui::window
{
// The Blood Castle / Chaos Castle time HUD (EventTimerView): the kill count, "Time left" and the
// time in the big font, each centred on the 124-unit frame and shrunk to it like the original's.
struct EventTimerRmlModel
{
    // The Hud layout's W/640 x H/480 stretch (UI::Scaling::GetActiveTransform() while CManager
    // runs the window) and its inverse for the counter-scaled text leaves.
    float scaleX = 1.f, scaleY = 1.f;
    float inverseScaleX = 1.f, inverseScaleY = 1.f;

    // The window's top-left, reference px (m_Pos).
    float panelX = 0.f, panelY = 0.f;

    Rml::String killsText; // empty: not drawn (no kill target received yet)
    float killsTextPx = 0.f;
    Rml::String timeLeftText;
    float timeLeftTextPx = 0.f;
    Rml::String timeText;
    float timeTextPx = 0.f;
    bool imminent = false; // under five minutes: the time in red (255, 32, 32)
};
} // namespace mu::ui::window
