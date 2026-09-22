#include "stdafx.h"
#include "App/Control/ControlModalFixture.h"
#include "App/Control/ControlProtocol.h"
#include "App/Control/ControlQuickPeer.h"
#include "App/Control/ControlUiReplay.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Dialogs/GenericMenuDialog.h"
#include "json.hpp"

#include <algorithm>

namespace
{
using nlohmann::json;
namespace Windows = mu::ui::window;
constexpr int FixtureVersion = 1;
constexpr std::size_t MaxNonceLength = 64;

Windows::CGenericMenuDialog* RegisteredMenu(bool worldReady)
{
    auto* menu = Windows::g_pGenericMenuDialog;
    if (!worldReady || menu == nullptr || g_pNewUISystem == nullptr || g_pNewUIMng == nullptr ||
        g_pNewUIMng->FindUIObj(Windows::INTERFACE_GENERIC_MENU_DIALOG) != menu)
        return nullptr;
    return menu;
}

json Inspect(std::string_view snapshot, Windows::CGenericMenuDialog* menu, std::string_view token)
{
    json state = {{"version", FixtureVersion},
                  {"ui", json::parse(snapshot)},
                  {"registered", menu != nullptr},
                  {"token_valid", nullptr},
                  {"active", nullptr},
                  {"queue_pending", nullptr},
                  {"click_pending", nullptr},
                  {"input_idle", App::Control::ControlInputIdle()}};
    if (menu != nullptr)
    {
        state["token_valid"] = menu->OwnsControlFixture(token);
        state["active"] = menu->IsVisible();
        state["queue_pending"] = menu->HasQueuedMenus();
        state["click_pending"] = menu->HasPendingClick();
    }
    return state;
}

bool ValidNonce(std::string_view nonce)
{
    return !nonce.empty() && nonce.size() <= MaxNonceLength &&
           std::all_of(nonce.begin(), nonce.end(),
                       [](unsigned char c)
                       {
                           return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                                  c == '-' || c == '_';
                       });
}
} // namespace

namespace App::Control
{
bool IsModalFixtureMutation(const Request& request)
{
    std::string action;
    return request.Command() == "ui" && request.GetString("action", action) &&
           (action == "fixture_create" || action == "fixture_retire");
}

std::string ModalFixture(const Request& request, std::string_view snapshot, bool worldReady, bool muted)
{
    std::string action;
    std::string token;
    (void)request.GetString("action", action);
    if (action != "fixture_inspect" && action != "fixture_create" && action != "fixture_retire")
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, "unknown fixture action");
    if (!request.GetString("token", token))
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, "fixture token required (empty for create)");
    auto* menu = RegisteredMenu(worldReady);
    const auto state = Inspect(snapshot, menu, token);
    if (action == "fixture_inspect")
    {
        auto result = state;
        result["guard"] = state.dump();
        return EncodeResult(request.EncodedId(), result.dump());
    }
    std::string guard;
    std::string nonce;
    int version = 0;
    bool optIn = false;
    if (!request.GetInt("fixture_version", version) || version != FixtureVersion ||
        !request.GetBool("fixture_opt_in", optIn) || !optIn || !request.GetString("guard", guard) ||
        (action == "fixture_create" && (!request.GetString("nonce", nonce) || !ValidNonce(nonce) || !token.empty())))
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, "invalid fixture contract");
    if (!worldReady || muted || menu == nullptr || guard != state.dump())
        return EncodeError(request.EncodedId(), ErrorCode::NotAllowed, "fixture state changed or unavailable");
    if (!state["input_idle"].get<bool>() || menu->HasQueuedMenus() || menu->HasPendingClick())
        return EncodeError(request.EncodedId(), ErrorCode::NotAllowed, "fixture input or queue pending");
    const bool retiring = action == "fixture_retire";
    if ((retiring && !menu->OwnsControlFixture(token)) || (!retiring && menu->IsVisible()))
        return EncodeError(request.EncodedId(), ErrorCode::NotAllowed, "fixture ownership mismatch");
    if (auto reason = UiFixtureRefusal(snapshot, retiring); !reason.empty())
        return EncodeError(request.EncodedId(), ErrorCode::NotAllowed, reason);
    if (retiring)
    {
        if (!menu->RetireControlFixture(token))
            return EncodeError(request.EncodedId(), ErrorCode::NotAllowed, "fixture ownership changed");
    }
    else
    {
        token = menu->CreateControlFixture(nonce);
        if (token.empty())
            return EncodeError(request.EncodedId(), ErrorCode::NotAllowed, "fixture creation refused");
    }
    return EncodeResult(
        request.EncodedId(),
        json({{"version", FixtureVersion}, {"token", token}, {"created", !retiring}, {"retired", retiring}}).dump());
}
} // namespace App::Control
