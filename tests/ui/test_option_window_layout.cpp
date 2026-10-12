#include <cstddef>
#include "doctest.h"

#include "UI/NewUI/Options/OptionWindowLayout.h"

using UI::Options::Box;
using UI::Options::CheckRow;

TEST_CASE("every checkbox row is clicked where it is drawn [ui][options]")
{
    for (const CheckRow& row : UI::Options::CheckRows())
    {
        CAPTURE(row.checkY);
        const Box box = UI::Options::CheckboxBox(row);
        // The hit box is the drawn box; the label and bullet sit within its height.
        CHECK(box.y == row.checkY);
        CHECK(row.labelY >= box.y);
        CHECK(row.labelY < box.Bottom());
        CHECK(row.bulletY >= box.y);
        CHECK(row.bulletY < box.Bottom());
    }
    // "Render full effects" used to be clicked 21 px below its drawn box.
    CHECK(UI::Options::CheckboxBox(UI::Options::RenderFullEffectsRow).y == 217);
}

TEST_CASE("no two controls of the options window overlap [ui][options]")
{
    constexpr auto boxes = UI::Options::HitBoxes();
    for (std::size_t a = 0; a < boxes.size(); ++a)
    {
        CAPTURE(a);
        // Inside the window and above the Close button.
        CHECK(boxes[a].x >= 0);
        CHECK(boxes[a].y >= 0);
        CHECK(boxes[a].x + boxes[a].width <= UI::Options::WindowWidth);
        CHECK(boxes[a].Bottom() <= UI::Options::CloseButton.y);
        CHECK_FALSE(boxes[a].Overlaps(UI::Options::CloseButton));
        for (std::size_t b = a + 1; b < boxes.size(); ++b)
        {
            CAPTURE(b);
            CHECK_FALSE(boxes[a].Overlaps(boxes[b]));
        }
    }
}

TEST_CASE("the window keeps its size and the Close button its place [ui][options]")
{
    // The window sits at y 5; growing it would push it further over the HUD.
    CHECK(UI::Options::WindowHeight == 418);
    CHECK(UI::Options::CloseButton.y == 388);
    CHECK(UI::Options::CloseButton.Bottom() == UI::Options::WindowHeight);
    CHECK(64 + UI::Options::FrameSlatCount * 10 + 45 <= UI::Options::WindowHeight);
}

TEST_CASE("Enhanced graphics sits in the graphics block under Render full effects [ui][options]")
{
    const Box effects = UI::Options::CheckboxBox(UI::Options::RenderFullEffectsRow);
    const Box enhanced = UI::Options::CheckboxBox(UI::Options::EnhancedGraphicsRow);
    CHECK(enhanced.y >= effects.Bottom());
    // The block's closing line lies under the new row and above the Font label.
    CHECK(UI::Options::GraphicsBlockLineY >= enhanced.Bottom());
    CHECK(UI::Options::GraphicsBlockLineY + 2 <= UI::Options::FontLabelY);
}

TEST_CASE("an open dropdown never covers the Close button and stays in the window [ui][options]")
{
    struct Combo
    {
        int labelY;
        int comboY;
        int maxVisible;
    };
    const Combo combos[] = {
        {UI::Options::FontLabelY, UI::Options::FontComboY, UI::Options::FontComboMaxVisible},
        {UI::Options::LanguageLabelY, UI::Options::LanguageComboY, UI::Options::LanguageComboMaxVisible},
        {UI::Options::ResolutionLabelY, UI::Options::ResolutionComboY, UI::Options::ResolutionComboMaxVisible},
    };
    for (const Combo& combo : combos)
    {
        CAPTURE(combo.comboY);
        // The label row lies between the field above and its own field.
        CHECK(combo.labelY + UI::Options::ComboLabelGap == combo.comboY);
        const bool upward = UI::Options::ComboOpensUpward(combo.comboY, combo.maxVisible);
        // Every list length up to the visible maximum, in the direction the combo opens.
        for (int rows = 1; rows <= combo.maxVisible; ++rows)
        {
            CAPTURE(rows);
            const Box list = UI::Options::ComboList(combo.comboY, rows, upward);
            CHECK_FALSE(list.Overlaps(UI::Options::CloseButton));
            CHECK(list.y >= 0);
            CHECK(list.Bottom() <= UI::Options::CloseButton.y);
        }
    }
    // Today's geometry: Font opens downward, Language and Resolution upward.
    CHECK_FALSE(UI::Options::ComboOpensUpward(UI::Options::FontComboY, UI::Options::FontComboMaxVisible));
    CHECK(UI::Options::ComboOpensUpward(UI::Options::LanguageComboY, UI::Options::LanguageComboMaxVisible));
    CHECK(UI::Options::ComboOpensUpward(UI::Options::ResolutionComboY, UI::Options::ResolutionComboMaxVisible));
}
