#pragma once

// On-screen notice banner: the scrolling system messages drawn over the HUD
// (events, server announcements, etc.). Extracted from ZzzInterface.cpp.
#include <functional>
#include <string_view>

namespace UI::Notices
{
// The auction window shows covered server notices as its own line (NoticeRouting.h); empty
// when it is gone.
void SetCoveringWindowLine(std::function<void(std::wstring_view text)> show);

// Push a notice line. Long text is split across two lines automatically.
void Create(const wchar_t* text, int color);

// Drop all current notices.
void Clear();

// Per-frame timing: expires the oldest notice over time.
void Move();

// Draw the active notices.
void Render();
}
