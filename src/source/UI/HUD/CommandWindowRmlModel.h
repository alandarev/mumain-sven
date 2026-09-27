#pragma once

#include <RmlUi/Core/Types.h>

#include <vector>

namespace mu::ui::window
{
// One of the twelve command buttons. `selected` is the armed command: the original drew its
// button pressed (frame 2) with a bold label until the command ran or was cancelled.
struct CommandButtonEntry
{
    Rml::String label;
    int index = 0; // COMMAND_TYPE
    bool selected = false;
};

struct CommandWindowRmlModel
{
    // Right-docked window -- UI::Scaling::GetActiveTransform() while CManager runs it.
    float rootX = 0.f, rootY = 0.f, rootScale = 1.f;
    float textPx = 0.f;     // native normal text size in physical px (RmlRootTransform.h)
    float boldTextPx = 0.f; // native bold text size (title, armed button)
    float bigTextPx = 0.f;  // native big text size (the target name at the pointer)

    Rml::String titleText;
    Rml::String exitTooltip;
    std::vector<CommandButtonEntry> buttons;

    // The target box the original drew at the pointer while a command is armed and a player is
    // under it: reference px relative to the panel, like everything else in it.
    bool targetVisible = false;
    float targetLeft = 0.f;
    float targetTop = 0.f;
    Rml::String targetName;
    bool targetInRange = false; // white name; red when the command cannot reach the player
};
} // namespace mu::ui::window
