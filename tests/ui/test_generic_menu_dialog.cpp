#include "stdafx.h"
#include "doctest.h"
#include "UI/Dialogs/GenericMenuDialog.h"

using mu::ui::window::CGenericMenuDialog;
using mu::ui::window::GenericMenuConfig;

TEST_CASE("Generic menu lifecycle restricts comparison cancellation to a lone system menu [ui][control-ui]")
{
    CGenericMenuDialog menu;
    CHECK_FALSE(menu.IsOnlySystemMenu());
    CHECK_FALSE(menu.DismissSystemMenu());
    int cancelled = 0;
    int chosen = 0;
    GenericMenuConfig system;
    system.purpose = GenericMenuConfig::Purpose::SystemMenu;
    system.onCancel = [&] { ++cancelled; };
    GenericMenuConfig::MenuButton button;
    button.onClick = [&] { ++chosen; };
    system.buttons.push_back(button);
    menu.Show(system);
    CHECK(menu.IsVisible());
    CHECK(menu.IsOnlySystemMenu());
    CHECK(menu.DismissSystemMenu());
    CHECK_FALSE(menu.IsVisible());
    CHECK_FALSE(menu.DismissSystemMenu());
    CHECK(cancelled == 1);
    CHECK(chosen == 0);

    menu.Show(GenericMenuConfig{});
    CHECK_FALSE(menu.IsOnlySystemMenu());
    CHECK_FALSE(menu.DismissSystemMenu());
    menu.Release();
    menu.Show(system);
    menu.Show(GenericMenuConfig{});
    CHECK_FALSE(menu.IsOnlySystemMenu());
    CHECK_FALSE(menu.DismissSystemMenu());
    CHECK(cancelled == 1);
    menu.Release();
    CHECK_FALSE(menu.IsVisible());
    menu.Show(system);
    CHECK(menu.IsOnlySystemMenu());
    CHECK(menu.DismissSystemMenu());
    CHECK(cancelled == 2);
    CHECK(chosen == 0);
}

TEST_CASE("Generic menu pending clicks and queue promotion preserve identity [ui][control-ui]")
{
    CGenericMenuDialog menu;
    int chosen = 0;
    int cancelled = 0;
    GenericMenuConfig system;
    system.purpose = GenericMenuConfig::Purpose::SystemMenu;
    system.onCancel = [&] { ++cancelled; };
    GenericMenuConfig::MenuButton button;
    button.onClick = [&] { ++chosen; };
    system.buttons.push_back(button);
    menu.OnButtonClicked(0); // No active dialog.
    menu.Show(system);
    menu.OnButtonClicked(-1);
    menu.OnButtonClicked(1);
    CHECK(menu.IsOnlySystemMenu());
    menu.OnButtonClicked(0);
    CHECK_FALSE(menu.IsOnlySystemMenu());
    CHECK_FALSE(menu.DismissSystemMenu());
    CHECK(chosen == 0);
    menu.Update();
    CHECK(chosen == 1);
    CHECK(cancelled == 0);
    CHECK_FALSE(menu.IsVisible());

    auto front = system;
    front.purpose = GenericMenuConfig::Purpose::Unspecified;
    menu.Show(front);
    menu.Show(system);
    CHECK_FALSE(menu.IsOnlySystemMenu());
    menu.OnButtonClicked(0);
    menu.Update();
    CHECK(chosen == 2);
    CHECK(menu.IsOnlySystemMenu());
    CHECK(menu.DismissSystemMenu());
    CHECK(cancelled == 1);

    system.buttons[0].enabled = false;
    menu.Show(system);
    menu.OnButtonClicked(0);
    menu.Update();
    CHECK(menu.IsOnlySystemMenu());
    CHECK(chosen == 2);
    menu.Release();
    CHECK_FALSE(menu.IsVisible());
    CHECK(cancelled == 1);
    CHECK(chosen == 2);
    system.buttons[0].enabled = true;
    menu.Show(system);
    menu.OnButtonClicked(0);
    menu.Release();
    menu.Update();
    CHECK_FALSE(menu.IsVisible());
    CHECK(cancelled == 1);
    CHECK(chosen == 2);
}
