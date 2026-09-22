#include "stdafx.h"
#include "App/Control/ControlModalFixture.h"
#include "App/Control/ControlProtocol.h"
#include "App/Control/ControlQuickPeer.h"
#include "App/Control/ControlUiReplay.h"
#include "UI/NewUI/NewUISystem.h"
#include "UI/NewUI/Dialogs/ControlModalMessageBox.h"
#include "json.hpp"

#include <algorithm>

namespace
{
using nlohmann::json;
namespace Windows = SEASON3B;
constexpr int FixtureVersion = 1;
constexpr std::size_t MaxNonceLength = 64;

Windows::CNewUIMessageBoxMng* RegisteredMenu(bool worldReady)
{
    auto* menu = g_MessageBox;
    if (!worldReady || menu == nullptr || g_pNewUISystem == nullptr || g_pNewUIMng == nullptr ||
        g_pNewUIMng->FindUIObj(Windows::INTERFACE_MESSAGEBOX) != menu || !menu->HasMessageBoxStorage())
        return nullptr;
    return menu;
}

json Inspect(std::string_view snapshot, Windows::CNewUIMessageBoxMng* menu, std::string_view token)
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
        state["token_valid"] = Windows::ControlModalMessageBox::Owns(*menu, token);
        state["active"] = !menu->IsEmpty();
        state["queue_pending"] = menu->HasPendingEvents();
        state["click_pending"] = menu->HasPendingPointerPress();
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
std::string ApplyFixture(const App::Control::Request& request, Windows::CNewUIMessageBoxMng& menu, bool retiring,
                         std::string token, std::string_view nonce)
{
    using namespace App::Control;
    if (retiring)
    {
        if (!Windows::ControlModalMessageBox::Retire(menu, token))
            return EncodeError(request.EncodedId(), ErrorCode::NotAllowed, "fixture ownership changed");
    }
    else
    {
        token = Windows::ControlModalMessageBox::CreateOwned(menu, nonce);
        if (token.empty())
            return EncodeError(request.EncodedId(), ErrorCode::NotAllowed, "fixture creation refused");
    }
    return EncodeResult(
        request.EncodedId(),
        json({{"version", FixtureVersion}, {"token", token}, {"created", !retiring}, {"retired", retiring}}).dump());
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
    if (!state["input_idle"].get<bool>() || menu->HasPendingEvents() || menu->HasPendingPointerPress())
        return EncodeError(request.EncodedId(), ErrorCode::NotAllowed, "fixture input or queue pending");
    const bool retiring = action == "fixture_retire";
    if ((retiring && !Windows::ControlModalMessageBox::Owns(*menu, token)) || (!retiring && !menu->IsEmpty()))
        return EncodeError(request.EncodedId(), ErrorCode::NotAllowed, "fixture ownership mismatch");
    if (auto reason = UiFixtureRefusal(snapshot, retiring); !reason.empty())
        return EncodeError(request.EncodedId(), ErrorCode::NotAllowed, reason);
    return ApplyFixture(request, *menu, retiring, std::move(token), nonce);
}
} // namespace App::Control
