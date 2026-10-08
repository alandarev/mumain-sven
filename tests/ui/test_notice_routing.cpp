#include <doctest.h>
#include "UI/HUD/NoticeRouting.h"

using namespace UI::Notices;

// The callers of UI::Notices::Create in WSclient.cpp and their colours: ReceiveNotice result 0
// (server-wide notice) uses 0; ReceiveNotice result 2 (guild notice), ReceiveGuildBeginWar,
// ReceiveGuildEndWar and ReceiveBattleCastleProcess (seal progress) use 1.
TEST_CASE("Notices covered by the auction window: server notices stay visible, the rest goes to chat [notices]")
{
    // Without the window, the banner as always.
    CHECK(RouteNotice(false, 0, L"Server restarts in 5 minutes") == NoticeRoute::Banner);
    CHECK(RouteNotice(false, 1, L"Guild war started") == NoticeRoute::Banner);
    // A server-wide notice may be an operator's restart warning: it becomes a window line.
    CHECK(RouteNotice(true, 0, L"Server restarts in 5 minutes") == NoticeRoute::WindowLineAndChat);
    CHECK(RouteNotice(true, 0, L"Chaos Castle entrance is open") == NoticeRoute::WindowLineAndChat);
    // Guild notices, guild war and castle siege progress go to the chat log.
    CHECK(RouteNotice(true, 1, L"[Guild] Meet at Lorencia") == NoticeRoute::Chat);
    // The periodic empty line that expires old banner lines never reaches the chat.
    CHECK(RouteNotice(true, 0, L"") == NoticeRoute::Banner);
    CHECK(RouteNotice(true, 1, L"") == NoticeRoute::Banner);
}

TEST_CASE("Notices keep their banner colour in the chat log [notices]")
{
    CHECK(ChatKind(0) == mu::ui::window::TYPE_GM_MESSAGE); // gold
    CHECK(ChatKind(1) == mu::ui::window::TYPE_GUILD_MESSAGE); // green
}
