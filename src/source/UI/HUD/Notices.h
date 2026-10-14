#pragma once

// On-screen notice banner: the scrolling system messages drawn over the HUD
// (events, server announcements, etc.). Extracted from ZzzInterface.cpp.
namespace UI::Notices
{
    // Push a notice line. Long text is split across two lines automatically.
    void Create(const wchar_t* text, int color);

    // Drop all current notices.
    void Clear();

    // Per-frame timing: expires the oldest notice over time.
    void Move();

    // Hold announcements while the compact HUD's assignment/key dialog owns input.
    void SetHudDialogOpen(bool open);

    // Hold announcements while the character Armory is being read or edited.
    void SetArmoryOpen(bool open);

    // Draw the active notices.
    void Render();
}
