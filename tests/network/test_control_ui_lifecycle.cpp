#include "stdafx.h"
#include "doctest.h"
#include "UiLifecycleFixture.h"
#include "App/Control/ControlCommands.h"
#include "App/Control/ControlUiObservability.h"
#include "App/Control/ControlQuickPeer.h"
#include "App/Control/QuickPeerSettleAct.h"
#include "World/MapInfra/MapManager.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "json.hpp"
#include <SDL3/SDL.h>

extern int g_iChatInputType;
extern DWORD g_dwTopWindow;
extern CHARACTER* CharacterMemoryDump;

namespace
{
using mu::ui::window::UiLifecycleFixture;
using nlohmann::json;

json List(const char* window = "chatinputbox")
{
    std::unique_ptr<App::Control::Act> act;
    json parameters = {{"cmd", "ui"}, {"id", 17}, {"action", "list"}};
    if (std::string_view(window) == "quick_command")
    {
        parameters["peer_key"] = 123;
        parameters["peer_id"] = "Peer";
    }
    const auto request = App::Control::Request::Parse(parameters.dump());
    const auto reply = json::parse(App::Control::Commands::Ui(request, act));
    REQUIRE(reply["ok"] == true);
    REQUIRE(act == nullptr);
    return reply["result"];
}

void RefuseWithoutMutation(const char* reason, const json& guard, const char* window = "chatinputbox")
{
    const auto before = List(window);
    std::unique_ptr<App::Control::Act> act;
    json parameters = {
        {"cmd", "ui"}, {"id", 18}, {"action", "guarded_hide"}, {"window", window}, {"guard", guard["guard"]}};
    if (std::string_view(window) == "quick_command")
    {
        parameters["peer_key"] = 123;
        parameters["peer_id"] = "Peer";
    }
    const auto request = App::Control::Request::Parse(parameters.dump());
    const auto reply = json::parse(App::Control::Commands::Ui(request, act));
    CHECK(reply["ok"] == false);
    CHECK(reply["error"] == "not_allowed");
    CHECK(reply["message"] == reason);
    CHECK(act == nullptr);
    CHECK(List(window) == before);
}

void RefuseStale(const json& guard, const char* window = "chatinputbox")
{
    REQUIRE(guard != List(window));
    RefuseWithoutMutation("UI state changed or unavailable", guard, window);
}

void RefuseFresh(const char* reason, const char* window = "chatinputbox")
{
    RefuseWithoutMutation(reason, List(window), window);
}

void PrepareModalPrerequisites(UiLifecycleFixture& fixture)
{
    REQUIRE(g_iChatInputType == 1);
    REQUIRE(g_dwTopWindow == 0);
    REQUIRE(g_pCryWolfInterface == nullptr);
    REQUIRE(g_pUIPopup == nullptr);
    fixture.PreparePopup();
    REQUIRE(g_MessageBox->IsEmpty());
    REQUIRE_FALSE(g_MessageBox->HasPendingEvents());
    REQUIRE(fixture.PrepareModalPrerequisites());
    const auto observation = List()["observability"];
    for (const char* field :
         {"legacy_popup_active", "message_window_active", "friend_children_active", "picked_item_active",
          "native_events_pending", "helper_active", "crywolf_event_active", "siegewarfare_child_active"})
    {
        CAPTURE(std::string(field));
        REQUIRE(observation[field] == false);
    }
}

void RequireUnownedSystem()
{
    REQUIRE(g_pNewUIMng == nullptr);
    REQUIRE(g_pSiegeWarfare == nullptr);
    REQUIRE(g_pChatInputBox == nullptr);
    REQUIRE(g_pQuickCommand == nullptr);
    REQUIRE(g_pNewUIHotKey == nullptr);
    REQUIRE_FALSE(CUITextInputBox::IsAnyInputBoxFocused());
}
} // namespace

TEST_CASE("Registered Siege child producer serializes empty populated retired and unavailable lifecycle "
          "[network][control-ui]")
{
    RequireUnownedSystem();
    const auto unavailable = App::Control::UiObservabilityObject();
    {
        UiLifecycleFixture fixture;
        PrepareModalPrerequisites(fixture);
        const auto empty = List();
        CHECK(empty["observability"]["siegewarfare_child_active"] == false);
        CHECK(fixture.siege.IsVisible());
        fixture.PopulateSiege();
        const auto populated = List();
        CHECK(populated["observability"]["siegewarfare_child_active"] == true);
        CHECK(fixture.siege.GetBase() != nullptr);
        RefuseStale(empty);
        RefuseFresh("active or unknown prerequisite: siegewarfare_child_active");
        fixture.siege.InitMiniMapUI();
        CHECK(fixture.siege.GetBase() == nullptr);
        CHECK(fixture.siege.IsVisible()); // Visibility is still not modal absence.
        CHECK(List()["observability"]["siegewarfare_child_active"] == false);
        RefuseStale(populated);
        fixture.registry.RemoveUIObj(mu::ui::window::INTERFACE_SIEGEWARFARE);
        const auto retired = List();
        CHECK(retired["observability"]["siegewarfare_child_active"].is_null());
        RefuseFresh("active or unknown prerequisite: siegewarfare_child_active");
        fixture.registry.AddUIObj(mu::ui::window::INTERFACE_SIEGEWARFARE, &fixture.siege);
        LoadingWorld = 30;
        CHECK(List()["observability"]["siegewarfare_child_active"].is_null());
    }
    CHECK(App::Control::UiObservabilityObject() == unavailable);
    RequireUnownedSystem();
}

TEST_CASE("Registered chat producer distinguishes owned whisper ordinary and unrelated input [network][control-ui]")
{
    RequireUnownedSystem();
    UiLifecycleFixture fixture;
    PrepareModalPrerequisites(fixture);
    const auto empty = List();
    for (const bool whisper : {false, true})
    {
        auto& input = fixture.Input(whisper);
        input.SetState(UISTATE_NORMAL);
        input.GiveFocus();
        CHECK(CUITextInputBox::GetFocusedPortable() == &input);
        const auto focused = List();
        CHECK(focused["observability"]["input_focused"] == true);
        CHECK(focused["observability"]["input_focus_owner"] == "chatinputbox");
        RefuseStale(empty);
        // Focus on either owned field blocks operations on unrelated panels.
        RefuseFresh("active or unknown prerequisite: input_focused", "help");
        fixture.registry.RemoveUIObj(mu::ui::window::INTERFACE_CHATINPUTBOX);
        CHECK(List()["observability"]["input_focus_owner"].is_null());
        RefuseStale(focused);
        RefuseFresh("active or unknown prerequisite: input_focused");
        fixture.registry.AddUIObj(mu::ui::window::INTERFACE_CHATINPUTBOX, &fixture.chat);
        input.SetState(UISTATE_HIDE);
        CHECK(List()["observability"]["input_focused"] == false);
    }
    CUITextInputBox unrelated;
    unrelated.SetState(UISTATE_NORMAL);
    unrelated.GiveFocus();
    const auto focused = List();
    CHECK(focused["observability"]["input_focused"] == true);
    CHECK(focused["observability"]["input_focus_owner"].is_null());
    RefuseFresh("active or unknown prerequisite: input_focused");
    unrelated.SetState(UISTATE_HIDE);
}

TEST_CASE("Registered Friends producer retains delayed message blocker after child retirement [network][control-ui]")
{
    RequireUnownedSystem();
    REQUIRE(g_iChatInputType == 1); // Ordinary Reset cannot send logout in this fixture.
    REQUIRE(g_dwTopWindow == 0);
    UiLifecycleFixture fixture;
    REQUIRE(g_pUIPopup == nullptr);
    fixture.PreparePopup();
    // Create only this subsystem, never the Friends main window or its network requests.
    auto destroy = [](mu::ui::window::CFriendWindow* friends)
    {
        delete friends;
        Core::Time::FrameTimerScheduler::Instance().Kill(CHATCONNECT_TIMER);
    };
    std::unique_ptr<mu::ui::window::CFriendWindow, decltype(destroy)> friends(new mu::ui::window::CFriendWindow,
                                                                              destroy);
    REQUIRE(friends->Create(&fixture.registry));
    // Create allocated a mutable manager owned solely by this test window.
    auto* manager = const_cast<CUIWindowMgr*>(friends->GetWindowManager());
    const auto empty = List();
    CHECK(empty["observability"]["friend_children_active"] == false);
    const auto child = manager->AddWindow(UIWNDTYPE_EMPTY, 0, 0, L"owned test child", 0, UIADDWND_FORCEPOSITION);
    REQUIRE(child != 0);
    CHECK(List()["observability"]["friend_children_active"] == true);
    RefuseFresh("active or unknown prerequisite: friend_children_active");
    manager->SendUIMessage(UI_MESSAGE_SELECT, 0, 0);
    manager->RemoveWindow(child);
    const auto delayed = List();
    CHECK(delayed["observability"]["friend_children_active"] == true);
    RefuseStale(empty);
    RefuseFresh("active or unknown prerequisite: friend_children_active");
    friends.reset();
    const auto unavailable = List();
    CHECK(unavailable["observability"]["friend_children_active"].is_null());
    RefuseFresh("active or unknown prerequisite: friend_children_active");
}

TEST_CASE("Quick producer drift reaches real JSON handler refusal and settlement without command execution "
          "[network][control-ui]")
{
    RequireUnownedSystem();
    REQUIRE(SDL_WasInit(SDL_INIT_EVENTS) == 0);
    REQUIRE_FALSE(MouseLButton);
    REQUIRE_FALSE(g_pNewKeyInput->HasPendingInput());
    UiLifecycleFixture fixture;
    auto storage = std::make_unique<CHARACTER[]>(MAX_CHARACTERS_CLIENT + 1 + 128);
    auto* savedOwner = CharacterMemoryDump;
    auto* savedCharacters = CharactersClient;
    auto* savedHero = Hero;
    const auto savedWidth = WindowWidth;
    const auto savedHeight = WindowHeight;
    const auto savedWorld = gMapManager.WorldActive;
    const auto savedMouseX = g_fWindowMouseX;
    const auto savedMouseY = g_fWindowMouseY;
    auto restore = [&](int*)
    {
        CharacterMemoryDump = savedOwner;
        CharactersClient = savedCharacters;
        Hero = savedHero;
        WindowWidth = savedWidth;
        WindowHeight = savedHeight;
        gMapManager.WorldActive = savedWorld;
        g_fWindowMouseX = savedMouseX;
        g_fWindowMouseY = savedMouseY;
        MouseLButton = false;
        g_pNewKeyInput->SetKeyState(VK_LBUTTON, mu::ui::window::CNewKeyInput::KEY_NONE);
        SDL_QuitSubSystem(SDL_INIT_EVENTS);
    };
    int ownerToken = 0;
    const std::unique_ptr<int, decltype(restore)> cleanup(&ownerToken, restore);
    REQUIRE(SDL_InitSubSystem(SDL_INIT_EVENTS)); // Isolated event queue, no window/device injection.
    CharacterMemoryDump = storage.get();
    CharactersClient = storage.get() + 127;
    Hero = CharactersClient;
    for (int index = 0; index < 2; ++index)
    {
        auto& character = CharactersClient[index];
        character.Object.Live = true;
        character.Object.Kind = KIND_PLAYER;
        character.Object.Type = MODEL_PLAYER;
        character.Object.SubType = 0;
        character.Key = index == 0 ? 122 : 123;
        wcscpy(character.ID, index == 0 ? L"Hero" : L"Peer");
        Vector(0.f, 0.f, 0.f, character.Object.Position);
    }
    WindowWidth = 1024;
    WindowHeight = 768;
    g_fWindowMouseX = 100.f;
    g_fWindowMouseY = 100.f;
    gMapManager.WorldActive = WD_0LORENCIA;
    const auto initial = List("quick_command");
    REQUIRE(initial["quick_peer"]["ready"] == true);
    REQUIRE(initial["quick_peer"]["input_idle"] == true);
    // The Act is production code reading production snapshots. Installation is
    // explicit, not a successful guarded OpenQuickCommand or renderer-frame test.
    for (const char* drift : {"geometry", "index", "peer_index", "context", "held", "edge", "queued", "unavailable"})
    {
        CAPTURE(drift);
        const auto before = List("quick_command");
        App::Control::QuickPeerSettleAct act(
            "{}", before.dump(),
            [] { return json::parse(App::Control::QuickPeerObservation(123, "Peer"))["ready"].get<bool>(); },
            [] { return List("quick_command").dump(); });
        std::string response;
        CHECK(act.Tick(response) == App::Control::Act::Status::Running);
        const std::string kind(drift);
        if (kind == "geometry")
            WindowWidth += 1;
        if (kind == "index")
            fixture.quick.SetSelectedCharacterIndex(2);
        if (kind == "peer_index")
        {
            CharactersClient[1].Object.Live = false;
            auto& moved = CharactersClient[2];
            moved.Object.Live = true;
            moved.Object.Kind = KIND_PLAYER;
            moved.Object.Type = MODEL_PLAYER;
            moved.Object.SubType = 0;
            moved.Key = 123;
            wcscpy(moved.ID, L"Peer");
            Vector(0.f, 0.f, 0.f, moved.Object.Position);
        }
        if (kind == "unavailable")
            fixture.registry.RemoveUIObj(mu::ui::window::INTERFACE_HOTKEY);
        if (kind == "context")
            gMapManager.WorldActive = WD_34CRYWOLF_1ST;
        if (kind == "held")
            MouseLButton = true; // Platform-input fixture, not physical SDL evidence.
        if (kind == "edge")
            g_pNewKeyInput->SetKeyState(VK_LBUTTON, mu::ui::window::CNewKeyInput::KEY_RELEASE);
        if (kind == "queued")
        {
            SDL_Event event{};
            event.type = SDL_EVENT_USER;
            REQUIRE(SDL_PushEvent(&event)); // Never dispatched to a game/input handler.
        }
        const auto changed = List("quick_command");
        CHECK(changed != before);
        if (kind == "held" || kind == "edge" || kind == "queued")
            CHECK(changed["quick_peer"]["input_idle"] == false);
        RefuseStale(before, "quick_command");
        CHECK(act.Tick(response) == App::Control::Act::Status::Finished);
        CHECK(json::parse(response)["error"] == "not_allowed");
        CHECK(List("quick_command") == changed);
        if (kind == "queued")
        {
            CHECK(SDL_HasEvent(SDL_EVENT_USER)); // Observation/guard/settle did not consume it.
            SDL_FlushEvent(SDL_EVENT_USER);      // This process owns the isolated test queue.
        }
        WindowWidth = 1024;
        CharactersClient[1].Object.Live = true;
        CharactersClient[2].Object.Live = false;
        if (kind == "unavailable")
            fixture.registry.AddUIObj(mu::ui::window::INTERFACE_HOTKEY, &fixture.hotkey);
        fixture.quick.SetSelectedCharacterIndex(-1);
        gMapManager.WorldActive = WD_0LORENCIA;
        MouseLButton = false;
        g_pNewKeyInput->SetKeyState(VK_LBUTTON, mu::ui::window::CNewKeyInput::KEY_NONE);
        CHECK(List("quick_command") == initial);
    }
}
