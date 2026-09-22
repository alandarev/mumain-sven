#include "stdafx.h"
#include "doctest.h"
#include "UI/Legacy/UIControls.h"
#include "UI/NewUI/Widgets/NewUIChatInputBox.h"

TEST_CASE("Uninitialized chat panel cannot claim another input field [ui][control-ui]")
{
    REQUIRE_FALSE(CUITextInputBox::IsAnyInputBoxFocused());
    SEASON3B::CNewUIChatInputBox chat;
    CHECK_FALSE(chat.OwnsFocusedInput());
    CUITextInputBox unrelated;
    unrelated.SetState(UISTATE_NORMAL);
    unrelated.GiveFocus();
    CHECK(CUITextInputBox::IsAnyInputBoxFocused());
    CHECK_FALSE(chat.OwnsFocusedInput());
    CHECK(CUITextInputBox::GetFocusedPortable() == &unrelated);
    unrelated.SetState(UISTATE_HIDE);
    CHECK_FALSE(chat.OwnsFocusedInput());
}
