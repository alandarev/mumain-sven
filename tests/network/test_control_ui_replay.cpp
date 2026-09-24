#include "doctest.h"
#include "App/Control/ControlUiReplay.h"
#include "json.hpp"

namespace
{
nlohmann::json Snapshot(bool rml = true)
{
    auto state = nlohmann::json{{"windows",
                                 {{{"window", "mainframe"}, {"visible", true}, {"registered", true}},
                                  {{"window", "messagebox"}, {"visible", true}, {"registered", true}}}},
                                {"observability",
                                 {{"version", 3},
                                  {"messagebox_active", false},
                                  {"native_events_pending", false},
                                  {"crywolf_event_active", false},
                                  {"siegewarfare_child_active", false},
                                  {"legacy_popup_active", false},
                                  {"message_window_active", false},
                                  {"friend_children_active", false},
                                  {"picked_item_active", false},
                                  {"system_menu_only", false},
                                  {"generic_dialogs_supported", rml},
                                  {"generic_confirm_active", rml ? nlohmann::json(false) : nlohmann::json(nullptr)},
                                  {"generic_menu_active", rml ? nlohmann::json(false) : nlohmann::json(nullptr)},
                                  {"inventory_open_effect", false},
                                  {"character_open_effect", false}}}};
    state["observability"]["helper_active"] = false;
    state["observability"]["input_focused"] = false;
    state["observability"]["friend_open_allowed"] = true;
    state["windows"] = nlohmann::json::array();
    // Source-derived registry, including hidden transactions; not runtime evidence.
    for (const char* name : {"friend",
                             "movemap",
                             "party",
                             "myquest",
                             "npcquest",
                             "guildinfo",
                             "trade",
                             "storage",
                             "storage_ext",
                             "mixinventory",
                             "command",
                             "pet",
                             "npcshop",
                             "inventory",
                             "inventory_ext",
                             "myshop_inventory",
                             "purchaseshop_inventory",
                             "character",
                             "npcbreeder",
                             "serverdivision",
                             "devilsquare",
                             "bloodcastle",
                             "npcguildmaster",
                             "guardsman",
                             "senatus",
                             "gatekeeper",
                             "gateswitch",
                             "catapult",
                             "refinery",
                             "refineryinfo",
                             "kanturu2nd_enternpc",
                             "cursedtemple_npc",
                             "cursedtemple_gamesystem",
                             "cursedtemple_result",
                             "chatinputbox",
                             "window_menu",
                             "option",
                             "help",
                             "item_explanation",
                             "setitem_explanation",
                             "quick_command",
                             "kanturu_info",
                             "chatlogwindow",
                             "party_info_window",
                             "bloodcastle_time",
                             "chaoscastle_time",
                             "battle_soccer_score",
                             "slidewindow",
                             "mu_helper_bar",
                             "hero_position_info",
                             "messagebox",
                             "duel_window",
                             "crywolf",
                             "name_window",
                             "siegewarfare",
                             "mainframe",
                             "skill_list",
                             "item_endurance_info",
                             "buff_window",
                             "master_level",
                             "gold_bowman",
                             "gold_bowman_lena",
                             "luckycoin_registration",
                             "exchange_luckycoin",
                             "duelwatch",
                             "duelwatch_mainframe",
                             "duelwatch_userlist",
                             "ingameshop",
                             "doppelganger_npc",
                             "doppelganger_frame",
                             "quest_progress",
                             "quest_progress_etc",
                             "empireguardian_npc",
                             "empireguardian_timer",
                             "mini_map",
                             "npc_dialogue",
                             "gensranking",
                             "unitedmarketplace_npc_julia",
                             "luckyitemwnd",
                             "hotkey",
                             "item_tooltip",
                             "muhelper",
                             "muhelper_ext",
                             "muhelper_skill_list",
                             "systemlogwindow",
                             "command_list",
                             "credits",
                             "server_message",
                             "server_select",
                             "msg_window",
                             "sys_menu",
                             "char_sel_main",
                             "char_make",
                             "login_main",
                             "login",
                             "char_info_balloon",
                             "generic_confirm_dialog",
                             "generic_menu_dialog"})
        state["windows"].push_back({{"window", name}, {"visible", false}, {"registered", true}});
    for (auto& entry : state["windows"])
    {
        for (const char* visible : {"mainframe", "skill_list", "hotkey", "chatlogwindow", "systemlogwindow",
                                    "slidewindow", "messagebox", "party_info_window", "name_window",
                                    "item_endurance_info", "buff_window", "mu_helper_bar", "crywolf", "siegewarfare"})
            if (entry["window"] == visible)
                entry["visible"] = true;
    }
    return state;
}

std::string Check(const nlohmann::json& snapshot, bool menu = false)
{
    return App::Control::UiReplayRefusal(snapshot.dump(), !menu, menu ? "system_menu" : "inventory", menu);
}
} // namespace

TEST_CASE("Replay policy refuses unknown and active observations [network][control-ui]")
{
    for (const bool rml : {false, true})
    {
        const auto clean = Snapshot(rml);
        CHECK(Check(clean).empty());
        for (const char* key :
             {"legacy_popup_active", "message_window_active", "friend_children_active", "picked_item_active",
              "native_events_pending", "inventory_open_effect", "helper_active", "input_focused"})
        {
            auto state = clean;
            state["observability"][key] = true;
            CHECK_FALSE(Check(state).empty());
            state["observability"][key] = nullptr;
            CHECK_FALSE(Check(state).empty());
            state["observability"].erase(key);
            CHECK_FALSE(Check(state).empty());
        }
        auto state = clean;
        state["observability"]["messagebox_active"] = true;
        CHECK_FALSE(Check(state).empty());
        state = clean;
        state["observability"]["version"] = 99;
        CHECK_FALSE(Check(state).empty());
    }
    CHECK_FALSE(App::Control::UiReplayRefusal("{}", true, "inventory", false).empty());
    CHECK_FALSE(App::Control::UiReplayRefusal("not JSON", true, "inventory", false).empty());
}

TEST_CASE("Replay policy distinguishes system menu from other modals [network][control-ui]")
{
    for (const bool rml : {false, true})
    {
        auto state = Snapshot(rml);
        state["observability"][rml ? "generic_menu_active" : "messagebox_active"] = true;
        CHECK_FALSE(Check(state, true).empty());
        state["observability"]["system_menu_only"] = true;
        CHECK(Check(state, true).empty());
        CHECK_FALSE(Check(state).empty());
        state["observability"]["legacy_popup_active"] = true;
        CHECK_FALSE(Check(state, true).empty());
    }
}

TEST_CASE("Replay policy does not whitelist transactions or malformed windows [network][control-ui]")
{
    auto state = Snapshot();
    state["windows"].push_back({{"window", "trade"}, {"visible", true}, {"registered", true}});
    CHECK_FALSE(Check(state).empty());
    state["windows"].back()["visible"] = false;
    CHECK(Check(state).empty());
    state["windows"].back().erase("registered");
    CHECK_FALSE(Check(state).empty());
    CHECK_FALSE(App::Control::UiReplayRefusal(Snapshot().dump(), false, "trade", false).empty());
}

TEST_CASE("Guarded dispatch never calls actions on stale unsafe or unavailable state [network][control-ui]")
{
    auto state = Snapshot();
    const auto guard = state.dump();
    int calls = 0;
    const auto action = [&]() -> std::string
    {
        ++calls;
        return {};
    };
    const auto run = [&](bool world, bool muted)
    { return App::Control::ExecuteGuardedUi(guard, state.dump(), world, muted, true, "inventory", action); };
    CHECK_FALSE(run(false, false).empty());
    CHECK_FALSE(run(true, true).empty());
    state["observability"]["inventory_open_effect"] = true;
    CHECK_FALSE(run(true, false).empty());
    CHECK_FALSE(
        App::Control::ExecuteGuardedUi(state.dump(), state.dump(), true, false, true, "inventory", action).empty());
    CHECK(calls == 0);
    state = Snapshot();
    CHECK(run(true, false).empty());
    CHECK(calls == 1);
    for (auto& entry : state["windows"])
        if (entry["window"] == "inventory")
            entry["registered"] = false;
    CHECK_FALSE(
        App::Control::ExecuteGuardedUi(state.dump(), state.dump(), true, false, true, "inventory", action).empty());
    CHECK(calls == 1);
}

TEST_CASE("Replay targets must be available natural transitions [network][control-ui]")
{
    auto state = Snapshot();
    CHECK_FALSE(App::Control::UiReplayRefusal(state.dump(), false, "myquest", false).empty());
    for (auto& entry : state["windows"])
        if (entry["window"] == "inventory")
            entry["visible"] = true;
    CHECK_FALSE(Check(state).empty());
    state = Snapshot();
    state["observability"]["friend_open_allowed"] = false;
    CHECK_FALSE(App::Control::UiReplayRefusal(state.dump(), true, "friend", false).empty());
    state["observability"]["friend_open_allowed"] = nullptr;
    CHECK_FALSE(App::Control::UiReplayRefusal(state.dump(), true, "friend", false).empty());
    state = Snapshot();
    for (auto& entry : state["windows"])
        if (entry["window"] == "duel_window")
            entry["visible"] = true;
    CHECK_FALSE(Check(state).empty());
    CHECK_FALSE(App::Control::UiReplayRefusal(Snapshot().dump(), false, "system_menu", true).empty());
}

TEST_CASE("Only closing chat input may keep its own identified focus [network][control-ui]")
{
    auto state = Snapshot();
    for (auto& entry : state["windows"])
        if (entry["window"] == "chatinputbox")
            entry["visible"] = true;
    state["observability"]["input_focused"] = true;
    state["observability"]["input_focus_owner"] = "chatinputbox";
    const auto check = [&] { return App::Control::UiReplayRefusal(state.dump(), false, "chatinputbox", false); };
    CHECK(check().empty());
    CHECK(App::Control::UiReplayRefusal(state.dump(), true, "chatinputbox", false) ==
          "active or unknown prerequisite: input_focused");
    CHECK(Check(state) == "active or unknown prerequisite: input_focused");
    for (const auto& owner : {nlohmann::json(nullptr), nlohmann::json("other"), nlohmann::json(true)})
    {
        state["observability"]["input_focus_owner"] = owner;
        CHECK(check() == "active or unknown prerequisite: input_focused");
    }
    state["observability"].erase("input_focus_owner");
    CHECK(check() == "active or unknown prerequisite: input_focused");
    state["observability"]["input_focus_owner"] = "chatinputbox";
    state["observability"]["input_focused"] = nullptr;
    CHECK(check() == "active or unknown prerequisite: input_focused");
    state["observability"]["input_focused"] = true;
    for (auto& entry : state["windows"])
        if (entry["window"] == "chatinputbox")
            entry["registered"] = false;
    CHECK(check() == "visible unregistered window");
}

TEST_CASE("Replay policy preserves menu modal and registry refusal reasons [network][control-ui]")
{
    for (const bool rml : {false, true})
    {
        auto state = Snapshot(rml);
        CHECK(App::Control::UiReplayRefusal(state.dump(), true, "system_menu", false).empty());
        state["observability"][rml ? "generic_menu_active" : "messagebox_active"] = true;
        state["observability"]["system_menu_only"] = true;
        const auto reason = rml ? "active or unknown modal" : "active or unknown native modal";
        CHECK(App::Control::UiReplayRefusal(state.dump(), true, "system_menu", true) == reason);
        int calls = 0;
        CHECK(App::Control::ExecuteGuardedUi(state.dump(), state.dump(), true, false, true, "system_menu",
                                             [&]() -> std::string
                                             {
                                                 ++calls;
                                                 return {};
                                             }) == reason);
        CHECK(calls == 0);
        state["observability"]["native_events_pending"] = true;
        CHECK(Check(state, true) == "active or unknown prerequisite: native_events_pending");
    }
    auto state = Snapshot();
    state["observability"]["generic_menu_active"] = true;
    state["observability"]["system_menu_only"] = true;
    state["observability"]["generic_confirm_active"] = true;
    CHECK(Check(state, true) == "active or unknown modal");
    state = Snapshot();
    for (auto& entry : state["windows"])
        if (entry["window"] == "option")
            entry["visible"] = true;
    CHECK(Check(state) == "unsupported visible window: option");
    state = Snapshot();
    state["windows"].push_back({{"window", "inventory"}, {"visible", false}, {"registered", true}});
    CHECK(Check(state) == "ambiguous panel");
    state["windows"] = nlohmann::json::array();
    CHECK(Check(state) == "missing registry");
    state["windows"] = nullptr;
    CHECK(Check(state) == "missing registry");
    state = Snapshot();
    state["observability"] = nullptr;
    CHECK(Check(state) == "unsupported UI observability");
}

TEST_CASE("Replay character quest and snapshot drift refuse before callbacks [network][control-ui]")
{
    auto state = Snapshot();
    for (const auto& effect : {nlohmann::json(true), nlohmann::json(nullptr)})
    {
        state["observability"]["character_open_effect"] = effect;
        CHECK(App::Control::UiReplayRefusal(state.dump(), true, "character", false) ==
              "opening may change quest state");
        CHECK(Check(state).empty());
    }
    state["observability"].erase("character_open_effect");
    CHECK(App::Control::UiReplayRefusal(state.dump(), true, "character", false) == "opening may change quest state");
    for (auto& entry : state["windows"])
        if (entry["window"] == "character")
            entry["visible"] = true;
    CHECK(App::Control::UiReplayRefusal(state.dump(), false, "character", false).empty());
    state = Snapshot();
    const auto guard = state.dump();
    int calls = 0;
    for (const char* key : {"theme", "ui_scale_percent"})
    {
        auto changed = state;
        changed[key] = "changed";
        CHECK(App::Control::ExecuteGuardedUi(guard, changed.dump(), true, false, true, "inventory",
                                             [&]() -> std::string
                                             {
                                                 ++calls;
                                                 return {};
                                             }) == "UI state changed or unavailable");
    }
    CHECK(calls == 0);
    CHECK(App::Control::ExecuteGuardedUi(guard, guard, true, false, true, "inventory", []() -> std::string
                                         { return "system menu changed"; }) == "system menu changed");
}

TEST_CASE("Schema 3 requires inactive event observations even for hidden managers [network][control-ui]")
{
    for (const bool rml : {false, true})
    {
        const auto clean = Snapshot(rml);
        CHECK(Check(clean).empty()); // Includes both inert, default-visible event managers.
        for (const char* key : {"crywolf_event_active", "siegewarfare_child_active"})
        {
            CAPTURE(key);
            for (const auto& invalid : {nlohmann::json(true), nlohmann::json(nullptr), nlohmann::json(0),
                                        nlohmann::json("false"), nlohmann::json::array(), nlohmann::json::object()})
            {
                auto state = clean;
                state["observability"][key] = invalid;
                CHECK(Check(state) == std::string("active or unknown prerequisite: ") + key);
                for (auto& entry : state["windows"])
                    if (entry["window"] == "crywolf" || entry["window"] == "siegewarfare")
                        entry["visible"] = false;
                CHECK_FALSE(Check(state).empty());
                int calls = 0;
                CHECK_FALSE(App::Control::ExecuteGuardedUi(state.dump(), state.dump(), true, false, true, "inventory",
                                                           [&]() -> std::string
                                                           {
                                                               ++calls;
                                                               return {};
                                                           })
                                .empty());
                CHECK(calls == 0);
            }
            auto state = clean;
            state["observability"].erase(key);
            CHECK_FALSE(Check(state).empty());
        }
        for (const auto& version : {nlohmann::json(2), nlohmann::json(4), nlohmann::json(nullptr), nlohmann::json("3"),
                                    nlohmann::json(3.0), nlohmann::json(true)})
        {
            auto state = clean;
            state["observability"]["version"] = version;
            CHECK(Check(state) == "unsupported UI observability");
        }
        auto state = clean;
        state["observability"].erase("version");
        CHECK(Check(state) == "unsupported UI observability");
    }
}

TEST_CASE("Quick contract is mandatory and snapshot drift never calls the action [network][control-ui]")
{
    auto state = Snapshot();
    int calls = 0;
    const auto action = [&]() -> std::string
    {
        ++calls;
        return {};
    };
    const auto execute = [&](const nlohmann::json& guard, const nlohmann::json& current)
    {
        return App::Control::ExecuteGuardedUi(guard.dump(), current.dump(), true, false, true, "quick_command", action);
    };
    CHECK_FALSE(execute(state, state).empty());
    // Policy fixtures only: production resolver/input/placement have separate coverage.
    state["quick_peer"] = {
        {"version", 1},          {"ready", true}, {"allowed", true}, {"input_idle", true}, {"placement_valid", true},
        {"menu_visible", false}, {"index", 1},    {"key", 123},      {"id", "Peer"},       {"placement", {20, 30}}};
    auto& quick = state["quick_peer"];
    for (const char* key :
         {"world", "hero_key", "hero_index", "kind", "type", "subtype", "menu_index", "command_index"})
        quick[key] = 0;
    quick["hero_id"] = "Hero";
    for (const char* key : {"menu_matches", "duel_watch", "chaos_castle", "cursed_temple", "strife"})
        quick[key] = false;
    quick["position"] = quick["hero_position"] = {0, 0, 0};
    quick["gens"] = {0, 0};
    quick["geometry"] = {1024, 768};
    quick["transform"] = quick["menu_transform"] = {1, 1, 0, 0, 1};
    quick["pointer"] = {10, 80, 10, 80};
    const auto clean = state;
    for (const char* field : {"ready", "allowed", "input_idle", "placement_valid"})
    {
        for (const auto& bad : {nlohmann::json(false), nlohmann::json(nullptr), nlohmann::json(0)})
        {
            state = clean;
            state["quick_peer"][field] = bad;
            CHECK_FALSE(execute(state, state).empty());
        }
        state["quick_peer"].erase(field);
        CHECK_FALSE(execute(state, state).empty());
    }
    for (const char* field : {"version", "index", "key", "id", "placement"})
    {
        state = clean;
        state["quick_peer"][field] = nullptr;
        CHECK_FALSE(execute(clean, state).empty());
    }
    for (auto it = clean["quick_peer"].begin(); it != clean["quick_peer"].end(); ++it)
    {
        state = clean;
        state["quick_peer"].erase(it.key());
        CHECK_FALSE(execute(state, state).empty());
    }
    CHECK(calls == 0);
    CHECK(execute(clean, clean).empty());
    CHECK(calls == 1);
    state = clean;
    for (auto& window : state["windows"])
        if (window["window"] == "quick_command")
            window["visible"] = true;
    state["quick_peer"]["menu_visible"] = true;
    state["quick_peer"]["menu_matches"] = true;
    state["quick_peer"]["menu_name"] = "Peer";
    state["quick_peer"]["menu_position"] = {20, 30};
    CHECK(App::Control::UiReplayRefusal(state.dump(), false, "quick_command", false).empty());
    state["quick_peer"]["menu_name"] = "Other";
    CHECK_FALSE(App::Control::UiReplayRefusal(state.dump(), false, "quick_command", false).empty());
}

TEST_CASE("Event field drift and an off-map Siege child refuse before action [network][control-ui]")
{
    for (const char* key : {"crywolf_event_active", "siegewarfare_child_active"})
    {
        const auto clean = Snapshot();
        int calls = 0;
        const auto action = [&]() -> std::string
        {
            ++calls;
            return {};
        };
        auto changed = clean;
        changed["observability"][key] = true;
        CHECK(App::Control::ExecuteGuardedUi(clean.dump(), changed.dump(), true, false, true, "inventory", action) ==
              "UI state changed or unavailable");
        changed["map"] = 0; // Policy fixture, not an actual populated-child producer test.
        CHECK_FALSE(
            App::Control::ExecuteGuardedUi(changed.dump(), changed.dump(), true, false, true, "inventory", action)
                .empty());
        changed["observability"][key] = nullptr;
        CHECK_FALSE(App::Control::ExecuteGuardedUi(clean.dump(), changed.dump(), true, false, true, "inventory", action)
                        .empty());
        changed["observability"].erase(key);
        CHECK_FALSE(App::Control::ExecuteGuardedUi(clean.dump(), changed.dump(), true, false, true, "inventory", action)
                        .empty());
        CHECK(calls == 0);
    }
}
