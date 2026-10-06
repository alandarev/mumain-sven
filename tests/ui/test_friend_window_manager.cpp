#include "stdafx.h"
#include "doctest.h"
#include "Core/Time/FrameTimerScheduler.h"
#include "UI/Social/FriendWindow.h"

extern int g_iChatInputType;
extern DWORD g_dwTopWindow;

TEST_CASE("Friend manager observes children and delayed messages [ui][control-ui]")
{
    REQUIRE(g_iChatInputType == 1);
    REQUIRE(g_dwTopWindow == 0);
    CUIWindowMgr manager;
    manager.Reset();
    CHECK_FALSE(manager.HasReplayBlockingChildren());
    const DWORD child = manager.AddWindow(UIWNDTYPE_CHAT_READY, 0, 0, L"local fixture", 0, UIADDWND_FORCEPOSITION);
    REQUIRE(child != 0);
    CHECK(manager.HasReplayBlockingChildren());
    manager.RemoveWindow(child);
    CHECK_FALSE(manager.HasReplayBlockingChildren());
    g_dwTopWindow = child;
    CHECK(manager.HasReplayBlockingChildren());
    g_dwTopWindow = 0;
    manager.SendUIMessage(UI_MESSAGE_SELECT, 0, 0);
    CHECK(manager.HasReplayBlockingChildren());
    Core::Time::FrameTimerScheduler::Instance().Kill(CHATCONNECT_TIMER);
}
