#include "stdafx.h"

#include "UI/Events/EntryDescription.h"

#include "Render/Text/TextWrap.h"

namespace UI::Events
{
namespace
{
constexpr int WrappedDescriptionLines = 2;
constexpr int WrappedDescriptionCapacity = 300;
} // namespace

void AppendWrappedDescription(std::vector<mu::ui::window::EventItemEntryView::Text>& texts, const wchar_t* description,
                              int wrapWidth, float boxWidth)
{
    wchar_t lines[WrappedDescriptionLines][WrappedDescriptionCapacity]{};
    CutStr(description, lines[0], wrapWidth, WrappedDescriptionLines, WrappedDescriptionCapacity);
    for (const auto& line : lines)
        texts.push_back({line, boxWidth});
}
} // namespace UI::Events
