#include "stdafx.h"
#include "doctest.h"
#include "UiLifecycleFixture.h"
#include "App/Control/ControlCommands.h"
#include "UI/Dialogs/GenericMenuDialog.h"
#include "UI/Dialogs/GenericConfirmDialog.h"
#include "Core/Input/SyntheticInput.h"
#include "json.hpp"
#include <SDL3/SDL.h>

namespace
{
using nlohmann::json;
namespace Windows = mu::ui::window;

json Call(json request)
{
    request["cmd"] = "ui";
    std::unique_ptr<App::Control::Act> act;
    const auto reply = json::parse(App::Control::Commands::Ui(App::Control::Request::Parse(request.dump()), act));
    CHECK(act == nullptr);
    return reply;
}
json Inspect(const std::string& token = {})
{
    const auto reply = Call({{"action", "fixture_inspect"}, {"token", token}});
    REQUIRE(reply["ok"] == true);
    return reply["result"];
}
json Mutation(const char* action, const std::string& token = {})
{
    return {{"action", action},       {"token", token},  {"fixture_version", 1},
            {"fixture_opt_in", true}, {"nonce", "test"}, {"guard", Inspect(token)["guard"]}};
}
void Refuse(json request, const char* reason)
{
    const auto token = request["token"].get<std::string>();
    const auto before = Inspect(token);
    const auto reply = Call(request);
    CHECK(reply["ok"] == false);
    CHECK(reply["message"] == reason);
    CHECK(Inspect(token) == before);
}
} // namespace

TEST_CASE(
    "Owned modal handler serializes normal fixed creation exact retirement and safety refusals [network][control-ui]")
{
    REQUIRE(g_pNewUIMng == nullptr);
    REQUIRE(Windows::g_pGenericMenuDialog == nullptr);
    REQUIRE_FALSE(SDL_WasInit(SDL_INIT_EVENTS));
    REQUIRE(SDL_Init(SDL_INIT_EVENTS));
    {
        Windows::UiLifecycleFixture fixture;
        fixture.PreparePopup();
        REQUIRE(fixture.PrepareModalPrerequisites());
        fixture.chat.CObject::Show(false);
        fixture.friends.CObject::Show(false);
        Windows::CGenericMenuDialog menu;
        Windows::CGenericConfirmDialog confirm;
        Windows::g_pGenericMenuDialog = &menu;
        menu.Create(&fixture.registry); // Normal registration; no RmlUi context/assets in this process.
        fixture.registry.AddUIObj(Windows::INTERFACE_GENERIC_CONFIRM_DIALOG, &confirm);
        const auto initial = Inspect();
        REQUIRE(initial["registered"] == true);
        REQUIRE(initial["input_idle"] == true);
        auto malformed = Mutation("fixture_create");
        malformed.erase("fixture_opt_in");
        Refuse(malformed, "invalid fixture contract");
        const auto created = Call(Mutation("fixture_create"));
        INFO(created.dump());
        REQUIRE(created["ok"] == true);
        const auto token = created["result"]["token"].get<std::string>();
        REQUIRE(Inspect(token)["token_valid"] == true);
        CHECK(Inspect(token)["ui"]["observability"]["generic_menu_active"] == true);
        CHECK(Inspect(token)["ui"]["observability"]["system_menu_only"] == false);
        Refuse(Mutation("fixture_retire", "stale"), "fixture ownership mismatch");
        const auto oldGuard = Mutation("fixture_retire", token);
        MouseLButton = true;
        Refuse(oldGuard, "fixture state changed or unavailable");
        Refuse(Mutation("fixture_retire", token), "fixture input or queue pending");
        MouseLButton = false;
        SDL_Event event{};
        event.type = SDL_EVENT_USER;
        REQUIRE(SDL_PushEvent(&event));
        Refuse(Mutation("fixture_retire", token), "fixture input or queue pending");
        CHECK(SDL_HasEvent(SDL_EVENT_USER));
        SDL_FlushEvent(SDL_EVENT_USER); // Only this isolated test's event.
        fixture.registry.RemoveUIObj(Windows::INTERFACE_GENERIC_MENU_DIALOG);
        CHECK(Inspect(token)["token_valid"].is_null());
        Refuse(Mutation("fixture_retire", token), "fixture state changed or unavailable");
        fixture.registry.AddUIObj(Windows::INTERFACE_GENERIC_MENU_DIALOG, &menu);
        REQUIRE(Call(Mutation("fixture_retire", token))["ok"] == true);
        CHECK_FALSE(menu.IsVisible());
        CHECK(Inspect(token)["token_valid"] == false);
        CHECK(fixture.registry.FindUIObj(Windows::INTERFACE_GENERIC_CONFIRM_DIALOG) == &confirm);
        Refuse(Mutation("fixture_retire", token), "fixture ownership mismatch");
        const auto again = Call(Mutation("fixture_create"));
        REQUIRE(again["ok"] == true);
        const auto second = again["result"]["token"].get<std::string>();
        CHECK(second != token);
        Windows::GenericMenuConfig unrelated;
        int callbacks = 0;
        unrelated.onCancel = [&] { ++callbacks; };
        menu.Show(unrelated);
        Refuse(Mutation("fixture_retire", second), "fixture input or queue pending");
        CHECK(menu.HasQueuedMenus());
        CHECK(callbacks == 0);
        menu.OnButtonClicked(0);
        Refuse(Mutation("fixture_retire", second), "fixture input or queue pending");
        menu.Update();
        CHECK(Inspect(second)["token_valid"] == false);
        Refuse(Mutation("fixture_retire", second), "fixture ownership mismatch");
        CHECK(menu.IsVisible());
        CHECK(callbacks == 0);
        menu.Release(); // Isolated owner teardown, not control cleanup.
        Windows::g_pGenericMenuDialog = nullptr;
    }
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
}
