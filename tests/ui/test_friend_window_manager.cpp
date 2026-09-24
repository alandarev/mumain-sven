#include "stdafx.h"
#include "doctest.h"
#include "Core/Time/FrameTimerScheduler.h"
#include "UI/Party/FriendWindow.h"

extern int g_iChatInputType;
extern DWORD g_dwTopWindow;

TEST_CASE("Friend manager observes children but not persistent tab finders [ui][control-ui]")
{
    REQUIRE(g_iChatInputType == 1); // Reset must not send a logout packet.
    REQUIRE(g_dwTopWindow == 0);
    auto* menu = mu::ui::window::CFriendWindow::GetFriendMenu();
    const int previousMenuState = menu->GetState();
    menu->SetState(UISTATE_HIDE);
    CUIWindowMgr manager;
    manager.Reset();
    CHECK_FALSE(manager.HasReplayBlockingChildren());

    // The same finder registration used by CUIFriendWindow::Init, without
    // creating the rendered main window or requesting a real friends list.
    CUITabWindow tab;
    manager.AddWindowFinder(&tab);
    CHECK_FALSE(manager.HasReplayBlockingChildren());
    const DWORD child = manager.AddWindow(UIWNDTYPE_EMPTY, 0, 0, L"local fixture", 0, UIADDWND_FORCEPOSITION);
    CHECK(child != 0);
    CHECK(manager.HasReplayBlockingChildren());
    manager.RemoveWindow(child);
    CHECK_FALSE(manager.HasReplayBlockingChildren());

    g_dwTopWindow = tab.GetUIID();
    CHECK(manager.HasReplayBlockingChildren());
    g_dwTopWindow = 0;
    menu->SetState(UISTATE_NORMAL);
    CHECK(manager.HasReplayBlockingChildren());
    menu->SetState(UISTATE_HIDE);
    CHECK_FALSE(manager.HasReplayBlockingChildren());
    manager.SendUIMessage(UI_MESSAGE_SELECT, 0, 0);
    CHECK(manager.HasReplayBlockingChildren());
    manager.RemoveWindowFinder(tab.GetUIID());
    Core::Time::FrameTimerScheduler::Instance().Kill(CHATCONNECT_TIMER);
    menu->SetState(previousMenuState);
}
