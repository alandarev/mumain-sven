#include "UI/HUD/NoticeRouting.h"

namespace UI::Notices
{
namespace
{
constexpr int ServerNoticeColor = 0;
}

NoticeRoute RouteNotice(bool coveredByWindow, int color, std::wstring_view text)
{
    if (!coveredByWindow || text.empty())
        return NoticeRoute::Banner;
    return color == ServerNoticeColor ? NoticeRoute::WindowLineAndChat : NoticeRoute::Chat;
}

mu::ui::window::MESSAGE_TYPE ChatKind(int color)
{
    return color == ServerNoticeColor ? mu::ui::window::TYPE_GM_MESSAGE : mu::ui::window::TYPE_GUILD_MESSAGE;
}
} // namespace UI::Notices
