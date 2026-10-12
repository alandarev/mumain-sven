#pragma once

#include <array>

// The rows of the options window (CNewUIOptionWindow), relative to its top-left corner. The window
// sits at y = 5 and is WindowHeight tall, so it must not grow: its bottom already touches the HUD.
// Drawing and hit testing read the same constants; tests/ui/test_option_window_layout.cpp checks
// that rows are clicked where they are drawn, that no two hit boxes overlap, that the Close button
// keeps its place and that no open dropdown covers it or leaves the window.
namespace UI::Options
{

struct Box
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    [[nodiscard]] constexpr int Bottom() const
    {
        return y + height;
    }
    [[nodiscard]] constexpr bool Overlaps(const Box& other) const
    {
        return x < other.x + other.width && other.x < x + width && y < other.y + other.height && other.y < y + height;
    }
};

inline constexpr int WindowWidth = 190;
inline constexpr int WindowHeight = 418;

// Every row starts with a bullet at BulletX and its label at LabelX.
inline constexpr int BulletX = 20;
inline constexpr int BulletSize = 10;
inline constexpr int LabelX = 40;

// Checkbox rows: the box is drawn and clicked at (CheckboxX, checkY), the label drawn at
// (LabelX, labelY) and the bullet at (BulletX, bulletY).
inline constexpr int CheckboxX = 150;
inline constexpr int CheckboxSize = 15;

struct CheckRow
{
    int checkY = 0;
    int labelY = 0;
    int bulletY = 0;
};

inline constexpr CheckRow AutoAttackRow{43, 48, 46};
inline constexpr CheckRow WhisperSoundRow{65, 70, 68};
inline constexpr CheckRow SlideHelpRow{155, 160, 158};
inline constexpr CheckRow RenderFullEffectsRow{217, 221, 219};
// "Enhanced graphics": the look preset, in the graphics block under "Render full effects".
inline constexpr CheckRow EnhancedGraphicsRow{236, 240, 238};
inline constexpr CheckRow WindowedRow{356, 361, 361};

[[nodiscard]] constexpr std::array<CheckRow, 6> CheckRows()
{
    return {{AutoAttackRow, WhisperSoundRow, SlideHelpRow, RenderFullEffectsRow, EnhancedGraphicsRow, WindowedRow}};
}

// The line that closes the graphics block, under its last row.
inline constexpr int GraphicsBlockLineY = 255;

// Volume sliders: the track, and its hit box, which starts VolumeSliderHitPadding left of it.
inline constexpr int VolumeSliderX = 33;
inline constexpr int VolumeSliderWidth = 124;
inline constexpr int VolumeSliderHitPadding = 8;
inline constexpr int VolumeSliderHitHeight = 16;
inline constexpr int SoundVolumeSliderY = 104;
inline constexpr int MusicVolumeSliderY = 132;

// Render level slider ("Effect limitation").
inline constexpr int RenderSliderX = 60;
inline constexpr int RenderSliderY = 191;
inline constexpr int RenderSliderWidth = 70;
inline constexpr int RenderSliderHeight = 15;

// Combo boxes: Font, Language, Resolution, each a label row (bullet at the label's y) and a field
// ComboLabelGap below it. Their pitch (32/31 px, was 39) is tighter than the rows above so the
// Enhanced graphics row fits without moving the Close button.
inline constexpr int ComboX = 22;
inline constexpr int ComboWidth = 148;
inline constexpr int ComboItemHeight = 16;
inline constexpr int ComboLabelGap = 13;
inline constexpr int FontLabelY = 263;
inline constexpr int FontComboY = FontLabelY + ComboLabelGap;
inline constexpr int FontComboMaxVisible = 5;
inline constexpr int LanguageLabelY = 295;
inline constexpr int LanguageComboY = LanguageLabelY + ComboLabelGap;
inline constexpr int LanguageComboMaxVisible = 5;
inline constexpr int ResolutionLabelY = 326;
inline constexpr int ResolutionComboY = ResolutionLabelY + ComboLabelGap;
inline constexpr int ResolutionComboMaxVisible = 4;

inline constexpr Box CloseButton{68, 388, 54, 30};

// The frame: a 64 px top, FrameSlatCount slats of 10 px and a 45 px bottom.
inline constexpr int FrameSlatCount = 30;

[[nodiscard]] constexpr Box CheckboxBox(const CheckRow& row)
{
    return {CheckboxX, row.checkY, CheckboxSize, CheckboxSize};
}

[[nodiscard]] constexpr Box VolumeSliderHitBox(int sliderY)
{
    return {VolumeSliderX - VolumeSliderHitPadding, sliderY, VolumeSliderWidth + VolumeSliderHitPadding,
            VolumeSliderHitHeight};
}

[[nodiscard]] constexpr Box ComboField(int comboY)
{
    return {ComboX, comboY, ComboWidth, ComboItemHeight};
}

// A dropdown opens upward when its longest list would reach the Close button below it.
[[nodiscard]] constexpr bool ComboOpensUpward(int comboY, int maxVisible)
{
    return comboY + ComboItemHeight + maxVisible * ComboItemHeight > CloseButton.y;
}

// The open list of a combo showing `rows` rows, below or above its field.
[[nodiscard]] constexpr Box ComboList(int comboY, int rows, bool upward)
{
    const int height = rows * ComboItemHeight;
    return {ComboX, upward ? comboY - height : comboY + ComboItemHeight, ComboWidth, height};
}

// Every control that takes a click, in window order.
[[nodiscard]] constexpr std::array<Box, 12> HitBoxes()
{
    return {{
        CheckboxBox(AutoAttackRow),
        CheckboxBox(WhisperSoundRow),
        VolumeSliderHitBox(SoundVolumeSliderY),
        VolumeSliderHitBox(MusicVolumeSliderY),
        CheckboxBox(SlideHelpRow),
        {RenderSliderX, RenderSliderY, RenderSliderWidth, RenderSliderHeight},
        CheckboxBox(RenderFullEffectsRow),
        CheckboxBox(EnhancedGraphicsRow),
        ComboField(FontComboY),
        ComboField(LanguageComboY),
        ComboField(ResolutionComboY),
        CheckboxBox(WindowedRow),
    }};
}

} // namespace UI::Options
