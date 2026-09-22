#include "stdafx.h"
#include "doctest.h"
#include "App/Control/ControlCommands.h"
#include "App/Control/ControlUiObservability.h"
#include "UI/NewUI/NewUISystem.h"
#include "World/MapInfra/MapManager.h"
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
    CHECK(state["version"] == 3);
    CHECK(state["generic_dialogs_supported"] == false);
    for (const char* key : {"messagebox_active", "native_events_pending", "friend_children_active", "system_menu_only",
                            "generic_confirm_active", "generic_menu_active", "input_focus_owner",
                            "crywolf_event_active", "siegewarfare_child_active"})
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

TEST_CASE("Event observation producers require exact registered managers and a ready world [network][control-ui]")
{
    namespace Windows = SEASON3B;
    Windows::CNewUIManager registry;
    Windows::CNewUICryWolf cryWolf;
    Windows::CNewUICryWolf otherCryWolf;
    Windows::CNewUISiegeWarfare siege;
    Windows::CNewUISiegeWarfare otherSiege;
    const auto cry = [&](Windows::CNewUIManager* manager, const Windows::CNewUICryWolf* expected, bool ready)
    { return App::Control::ObserveCryWolfEvent(manager, expected, ready); };
    const auto child = [&](Windows::CNewUIManager* manager, Windows::CNewUISiegeWarfare* expected, bool ready)
    { return App::Control::ObserveSiegeWarfareChild(manager, expected, ready); };

    CHECK_FALSE(cry(nullptr, &cryWolf, true).has_value());
    CHECK_FALSE(child(nullptr, &siege, true).has_value());
    CHECK_FALSE(cry(&registry, &cryWolf, true).has_value());
    CHECK_FALSE(child(&registry, &siege, true).has_value());
    registry.AddUIObj(Windows::INTERFACE_CRYWOLF, &cryWolf);
    registry.AddUIObj(Windows::INTERFACE_SIEGEWARFARE, &siege);
    CHECK_FALSE(cry(&registry, nullptr, true).has_value());
    CHECK_FALSE(child(&registry, nullptr, true).has_value());
    CHECK_FALSE(cry(&registry, &otherCryWolf, true).has_value());
    CHECK_FALSE(child(&registry, &otherSiege, true).has_value());
    CHECK_FALSE(cry(&registry, &cryWolf, false).has_value());
    CHECK_FALSE(child(&registry, &siege, false).has_value());
    CHECK(cryWolf.IsVisible());
    CHECK(siege.IsVisible());
    CHECK(siege.IsCreated()); // Deliberately true despite the absent child.
    CHECK(siege.GetBase() == nullptr);

    const int savedWorld = gMapManager.WorldActive;
    // No fatal assertions after changing the fixture global; restore it below.
    for (const int world : {WD_0LORENCIA, WD_34CRYWOLF_1ST, WD_30BATTLECASTLE, WD_0LORENCIA})
    {
        gMapManager.WorldActive = world;
        for (int read = 0; read < 2; ++read)
        {
            CHECK(cry(&registry, &cryWolf, true) == std::optional<bool>(world == WD_34CRYWOLF_1ST));
            CHECK(child(&registry, &siege, true) == std::optional<bool>(false));
            CHECK(gMapManager.WorldActive == world);
            CHECK(cryWolf.IsVisible());
            CHECK(siege.IsVisible());
            CHECK(siege.GetBase() == nullptr);
        }
    }
    gMapManager.WorldActive = savedWorld;
    registry.RemoveUIObj(Windows::INTERFACE_CRYWOLF);
    registry.RemoveUIObj(Windows::INTERFACE_SIEGEWARFARE);
    CHECK_FALSE(cry(&registry, &cryWolf, true).has_value());
    CHECK_FALSE(child(&registry, &siege, true).has_value());
}
