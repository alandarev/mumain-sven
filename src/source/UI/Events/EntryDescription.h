#pragma once

#include "UI/Events/EventItemEntryView.h"

#include <vector>

namespace UI::Events
{
void AppendWrappedDescription(std::vector<mu::ui::window::EventItemEntryView::Text>& texts, const wchar_t* description,
                              int wrapWidth, float boxWidth);
}
