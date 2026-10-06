#include "stdafx.h"
#include "doctest.h"
#include "../network/UiLifecycleFixture.h"

TEST_CASE("Chat panel cannot claim an unrelated input field [ui][control-ui]")
{
    mu::ui::window::UiLifecycleFixture fixture;
    CHECK_FALSE(fixture.chat.OwnsFocusedInput());
    auto* unrelated = fixture.UnrelatedInput();
    REQUIRE(unrelated != nullptr);
    REQUIRE(unrelated->Focus());
    fixture.Settle();
    CHECK_FALSE(fixture.chat.OwnsFocusedInput());
    unrelated->Blur();
}
