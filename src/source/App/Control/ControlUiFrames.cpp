#include "App/Control/ControlUiFrames.h"
#include <limits>

namespace
{
std::uint64_t completed = 0;
bool updating = false;
bool rendered = false;
} // namespace

namespace App::Control
{
void BeginUiFrame()
{
    updating = false;
    rendered = false;
}
void ObserveUiUpdate()
{
    updating = true;
}
void ObserveUiRender()
{
    rendered = updating;
}
void CompleteUiFrame()
{
    if (rendered && completed != std::numeric_limits<std::uint64_t>::max())
        ++completed;
    BeginUiFrame();
}
std::uint64_t CompletedUiFrames()
{
    return completed;
}
} // namespace App::Control
