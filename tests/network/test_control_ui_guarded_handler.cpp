#include "stdafx.h"
#include "doctest.h"
#include "App/Control/ControlCommands.h"
#include "App/Control/ControlUiObservability.h"
#include "UI/NewUI/NewUISystem.h"
#include "json.hpp"

using nlohmann::json;

TEST_CASE("UI handler preserves unavailable-world refusal before dispatch [network][control-ui]")
{
    REQUIRE(g_pNewUIMng == nullptr);
    const auto before = App::Control::UiObservabilityObject();
    for (const char* action : {"list", "guarded_show", "guarded_hide", "show", "hide", "toggle"})
    {
        CAPTURE(action);
        std::unique_ptr<App::Control::Act> act;
        const auto request = App::Control::Request::Parse(json({{"cmd", "ui"}, {"id", 7}, {"action", action}}).dump());
        REQUIRE(request.IsValid());
        const auto response = json::parse(App::Control::Commands::Ui(request, act));
        CHECK(response["id"] == 7);
        CHECK(response["ok"] == false);
        CHECK(response["error"] == "wrong_scene");
        CHECK(response["message"] == "the main-scene windows exist only in the world");
        CHECK(act == nullptr);
        CHECK(g_pNewUIMng == nullptr);
        CHECK(App::Control::UiObservabilityObject() == before);
    }
    std::unique_ptr<App::Control::Act> act;
    const auto response = json::parse(App::Control::Commands::Ui(App::Control::Request::Parse(R"({"cmd":"ui"})"), act));
    CHECK(response["error"] == "bad_request");
    CHECK(act == nullptr);
}

TEST_CASE("Real observation producer reports unavailable managers without mutation [network][control-ui]")
{
    REQUIRE(g_pNewUIMng == nullptr);
    const auto first = App::Control::UiObservabilityObject();
    const auto state = json::parse(first);
    CHECK(state["version"] == 2);
    CHECK(state["generic_dialogs_supported"] == false);
    for (const char* key : {"messagebox_active", "native_events_pending", "friend_children_active", "system_menu_only",
                            "generic_confirm_active", "generic_menu_active", "input_focus_owner"})
    {
        CAPTURE(key);
        REQUIRE(state.contains(key));
        CHECK(state[key].is_null());
    }
    for (const char* key : {"helper_active", "input_focused", "message_window_active", "picked_item_active",
                            "inventory_open_effect", "character_open_effect"})
    {
        CAPTURE(key);
        REQUIRE(state.contains(key));
        CHECK(state[key].is_boolean());
    }
    CHECK(state.contains("legacy_popup_active"));
    CHECK(state.contains("friend_open_allowed"));
    CHECK(App::Control::UiObservabilityObject() == first);
    CHECK(g_pNewUIMng == nullptr);
}
