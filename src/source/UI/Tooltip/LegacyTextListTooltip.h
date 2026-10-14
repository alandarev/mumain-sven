#pragma once

#include <optional>

namespace UI::Tooltip
{
enum class Placement
{
    Below,
    Above,
};

// The item information tooltip's owner in the shared tooltip (RenderItemInfo(), RenderRepairInfo(),
// the pet's): the default below, so a window that stops showing an item hides that and nothing else.
const void* ItemInfoOwner();

// Presents the current TextList/TextListColor/TextBold contents as a hover tooltip.
// The anchor is already in screen pixels; callers convert from their own coordinate space.
// `flipAnchorY`: the edge a box that leaves the viewport takes on the other side (a cursor-following
// item tooltip flips above the cursor's top, not through it). Without one it flips at `anchorY`.
void ShowLegacyTextList(int lineCount, float anchorX, float anchorY, Placement placement = Placement::Below,
                        const void* owner = ItemInfoOwner(), std::optional<float> flipAnchorY = std::nullopt);
void HideLegacyTextList(const void* owner = ItemInfoOwner());
}
