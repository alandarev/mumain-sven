#include "stdafx.h"
#include "doctest.h"
#include "UI/Dialogs/GenericMenuDialog.h"
#include <optional>

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

#if MU_ENABLE_CONTROL_SOCKET
TEST_CASE("Owned fixed menu config invalidates on ordinary close Release and same-address reuse [ui][control-ui]")
{
    // Normal Show/Resolve paths, without RmlUi assets or a rendered-document claim.
    std::optional<CGenericMenuDialog> menu(std::in_place);
    const auto* address = &*menu;
    const auto first = menu->CreateControlFixture("one");
    REQUIRE_FALSE(first.empty());
    CHECK(menu->OwnsControlFixture(first));
    CHECK_FALSE(menu->IsOnlySystemMenu());
    CHECK_FALSE(menu->RetireControlFixture("wrong"));
    CHECK(menu->RetireControlFixture(first));
    CHECK_FALSE(menu->OwnsControlFixture(first));
    const auto second = menu->CreateControlFixture("one");
    REQUIRE_FALSE(second.empty());
    CHECK(first != second);
    menu->Release(); // m_Active content remains stored; ownership must not.
    CHECK_FALSE(menu->OwnsControlFixture(second));
    menu->Show(GenericMenuConfig{});
    CHECK_FALSE(menu->RetireControlFixture(second));
    menu->Release();
    const auto destroyed = menu->CreateControlFixture("one");
    REQUIRE(menu->OwnsControlFixture(destroyed));
    menu.reset(); // Destroy an actively owned config, not an already retired one.
    menu.emplace();
    REQUIRE(&*menu == address);
    const auto third = menu->CreateControlFixture("one");
    CHECK(third != destroyed);
    CHECK_FALSE(menu->RetireControlFixture(destroyed));
    CHECK_FALSE(menu->RetireControlFixture(second));
    CHECK(menu->RetireControlFixture(third));
}

TEST_CASE("Owned fixture retirement preserves unrelated queued config and pending click [ui][control-ui]")
{
    CGenericMenuDialog menu;
    const auto token = menu.CreateControlFixture("queued");
    REQUIRE_FALSE(token.empty());
    int cancelled = 0;
    GenericMenuConfig unrelated;
    unrelated.purpose = GenericMenuConfig::Purpose::SystemMenu;
    unrelated.onCancel = [&] { ++cancelled; };
    menu.Show(unrelated);
    CHECK(menu.HasQueuedMenus());
    CHECK_FALSE(menu.RetireControlFixture(token));
    CHECK(menu.OwnsControlFixture(token));
    CHECK(cancelled == 0);
    menu.OnButtonClicked(0);
    CHECK(menu.HasPendingClick());
    CHECK_FALSE(menu.RetireControlFixture(token));
    menu.Update(); // Ordinary inert close promotes, but never cancels, unrelated state.
    CHECK_FALSE(menu.OwnsControlFixture(token));
    CHECK_FALSE(menu.RetireControlFixture(token));
    CHECK(menu.IsOnlySystemMenu());
    CHECK(cancelled == 0);
    menu.Release(); // Isolated test teardown, never the fixture retirement implementation.
}
#endif
