#pragma once

#include "UI/Chat/MessageType.h"

#include <cstdint>
#include <string_view>

namespace UI::Notices
{
// Where a banner notice goes while the auction window covers the banner area (D24). Colour 0 is
// the server-wide notice (packet 0D, result 0): the server sends operator and GM announcements,
// event openings and castle siege countdowns all as that one kind, so an operator's restart or
// maintenance warning cannot be told apart from an event opening. The safer choice keeps every
// colour 0 notice visible, as a line in the auction window. Colour 1 (guild notices, guild war
// start and end, castle siege seal progress) goes to the chat log.
enum class NoticeRoute : std::uint8_t
{
    // The banner itself; also the empty lines that let older notices expire.
    Banner,
    // A line in the covering window, and the chat log for the record.
    WindowLineAndChat,
    Chat
};

NoticeRoute RouteNotice(bool coveredByWindow, int color, std::wstring_view text);
// The chat kind that keeps the banner colour: gold for server notices, green for the others.
mu::ui::window::MESSAGE_TYPE ChatKind(int color);
} // namespace UI::Notices
