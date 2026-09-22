#include "stdafx.h"
#include "doctest.h"
#include "UiLifecycleFixture.h"
#include "App/Control/ControlDispatcher.h"
#include "UI/NewUI/Dialogs/ControlModalMessageBox.h"
#include "json.hpp"
#include <SDL3/SDL.h>
#include <new>

namespace SEASON3B
{
// Installs only isolated native storage; ordinary Release runs every destructor
// and its absent-texture deletes under UiLifecycleFixture's NativeAssetBoundary.
struct MessageBoxLifecycleFixture
{
    CNewUIMessageBoxMng& manager = *g_MessageBox;
    MessageBoxLifecycleFixture()
    {
        REQUIRE(manager.m_pMsgBoxFactory == nullptr);
        REQUIRE(manager.IsEmpty());
        REQUIRE_FALSE(manager.HasPendingEvents());
        manager.m_pMsgBoxFactory = new CNewUIMessageBoxFactory;
        manager.m_EventState = CNewUIMessageBoxMng::EVENT_NONE;
    }
    ~MessageBoxLifecycleFixture()
    {
        manager.Release();
    }
    void SetPointerPress(bool down)
    {
        manager.m_EventState =
            down ? CNewUIMessageBoxMng::EVENT_WND_MOUSE_LBUTTON_DOWN : CNewUIMessageBoxMng::EVENT_NONE;
    }
};
} // namespace SEASON3B

namespace
{
using nlohmann::json;
using namespace SEASON3B;
json Call(json request)
{
    request["cmd"] = "ui";
    App::Control::Dispatcher dispatcher;
    dispatcher.Handle(App::Control::Request::Parse(request.dump()), 1);
    CHECK_FALSE(dispatcher.HasActInFlight());
    const auto replies = dispatcher.TakeResponses();
    REQUIRE(replies.size() == 1);
    return json::parse(replies.front().line);
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
std::string Create()
{
    const auto result = Call(Mutation("fixture_create"));
    INFO(result.dump());
    REQUIRE(result["ok"] == true);
    return result["result"]["token"].get<std::string>();
}
int callbacks = 0;
CALLBACK_RESULT UnrelatedCallback(CNewUIMessageBoxBase*, const leaf::xstreambuf&)
{
    ++callbacks;
    return CALLBACK_CONTINUE;
}
} // namespace

TEST_CASE("Native owned modal dispatcher handler producer JSON lifecycle and exact refusals [network][control-ui]")
{
    REQUIRE(g_pNewUIMng == nullptr);
    REQUIRE_FALSE(SDL_WasInit(SDL_INIT_EVENTS));
    REQUIRE(SDL_Init(SDL_INIT_EVENTS));
    {
        SEASON3B::UiLifecycleFixture fixture;
        fixture.PreparePopup();
        REQUIRE(fixture.PrepareModalPrerequisites());
        fixture.chat.CNewUIObj::Show(false);
        fixture.friends.CNewUIObj::Show(false);
        SEASON3B::MessageBoxLifecycleFixture storage;
        auto& manager = storage.manager;
        REQUIRE(Inspect()["registered"] == true);
        REQUIRE(Inspect()["input_idle"] == true);
        auto malformed = Mutation("fixture_create");
        malformed.erase("fixture_opt_in");
        Refuse(malformed, "invalid fixture contract");
        malformed = Mutation("fixture_create");
        malformed["fixture_version"] = 2;
        Refuse(malformed, "invalid fixture contract");
        malformed = Mutation("fixture_create");
        auto guard = json::parse(malformed["guard"].get<std::string>());
        guard["ui"]["observability"].erase("native_events_pending");
        malformed["guard"] = guard.dump();
        Refuse(malformed, "fixture state changed or unavailable");
        {
            App::Control::Dispatcher dispatcher;
            dispatcher.Handle(
                App::Control::Request::Parse(R"({"cmd":"ui","action":"show","window":"hotkey","raw":true})"), 1);
            REQUIRE(dispatcher.HasActInFlight());
            const auto before = Inspect();
            auto request = Mutation("fixture_create");
            request["cmd"] = "ui";
            dispatcher.Handle(App::Control::Request::Parse(request.dump()), 2);
            const auto replies = dispatcher.TakeResponses();
            REQUIRE(replies.size() == 1);
            CHECK(json::parse(replies.front().line)["message"] == "control action pending");
            CHECK(dispatcher.HasActInFlight());
            CHECK(Inspect() == before);
            dispatcher.AbandonConnection(1);
        }
        MouseLButton = true;
        Refuse(Mutation("fixture_create"), "fixture input or queue pending");
        MouseLButton = false;
        const auto unavailableGuard = Mutation("fixture_create");
        LoadingWorld = 30;
        CHECK(Inspect()["token_valid"].is_null());
        Refuse(unavailableGuard, "fixture state changed or unavailable");
        Refuse(Mutation("fixture_create"), "fixture state changed or unavailable");
        LoadingWorld = 0;
        const auto token = Create();
        REQUIRE(Inspect(token)["token_valid"] == true);
        CHECK(Inspect(token)["ui"]["observability"]["messagebox_active"] == true);
        CHECK(Inspect(token)["ui"]["observability"]["system_menu_only"] == false);
        const auto beforeGuarded = Inspect(token);
        const auto guarded =
            Call({{"action", "guarded_show"}, {"window", "chatinputbox"}, {"guard", beforeGuarded["ui"].dump()}});
        CHECK(guarded["ok"] == false);
        CHECK(guarded["message"] == "active or unknown native modal");
        CHECK(Inspect(token) == beforeGuarded);
        auto* owned = manager.GetMessageBoxes().front();
        for (DWORD event : {MSGBOX_EVENT_PRESSKEY_ESC, MSGBOX_EVENT_MOUSE_LBUTTON_UP, MSGBOX_EVENT_USER_COMMON_OK})
            CHECK(owned->GetCallbackFunc(event) == nullptr);
        Refuse(Mutation("fixture_retire", "stale"), "fixture ownership mismatch");
        const auto oldGuard = Mutation("fixture_retire", token);
        MouseLButton = true;
        Refuse(oldGuard, "fixture state changed or unavailable");
        Refuse(Mutation("fixture_retire", token), "fixture input or queue pending");
        MouseLButton = false;
        storage.SetPointerPress(true);
        Refuse(Mutation("fixture_retire", token), "fixture input or queue pending");
        storage.SetPointerPress(false);
        SDL_Event event{};
        event.type = SDL_EVENT_USER;
        REQUIRE(SDL_PushEvent(&event));
        Refuse(Mutation("fixture_retire", token), "fixture input or queue pending");
        CHECK(SDL_HasEvent(SDL_EVENT_USER));
        SDL_FlushEvent(SDL_EVENT_USER); // Only this isolated test's event.
        fixture.registry.RemoveUIObj(INTERFACE_MESSAGEBOX);
        CHECK(Inspect(token)["token_valid"].is_null());
        Refuse(Mutation("fixture_retire", token), "fixture state changed or unavailable");
        fixture.registry.AddUIObj(INTERFACE_MESSAGEBOX, &fixture.hotkey);
        Refuse(Mutation("fixture_retire", token), "fixture state changed or unavailable");
        fixture.registry.RemoveUIObj(INTERFACE_MESSAGEBOX);
        fixture.registry.AddUIObj(INTERFACE_MESSAGEBOX, &manager);
        REQUIRE(Call(Mutation("fixture_retire", token))["ok"] == true);
        CHECK(manager.IsEmpty());
        CHECK_FALSE(manager.HasPendingEvents());
        CHECK(Inspect(token)["token_valid"] == false);
        CHECK(fixture.registry.FindUIObj(INTERFACE_HOTKEY) == &fixture.hotkey);
        Refuse(Mutation("fixture_retire", token), "fixture ownership mismatch");
        const auto second = Create();
        CHECK(second != token);
        owned = manager.GetMessageBoxes().front();
        auto* unrelated = manager.NewMessageBox(MSGBOX_CLASS(CNewUICommonMessageBox));
        REQUIRE(unrelated->Create(MSGBOX_COMMON_TYPE_OK));
        callbacks = 0;
        unrelated->AddCallbackFunc(UnrelatedCallback, MSGBOX_EVENT_USER_DEFINE);
        Refuse(Mutation("fixture_retire", second), "fixture ownership mismatch");
        manager.SendEvent(unrelated, MSGBOX_EVENT_USER_DEFINE);
        Refuse(Mutation("fixture_retire", second), "fixture input or queue pending");
        CHECK(callbacks == 0);
        CHECK(manager.GetMessageBoxes().size() == 2);
        CHECK(manager.HasPendingEvents());
        // Ordinary exact deletion outside control proves unrelated event preservation.
        manager.DeleteMessageBox(owned);
        CHECK(manager.HasPendingEvents());
        manager.Update();
        CHECK(callbacks == 1);
        CHECK(manager.GetMessageBoxes().front() == unrelated);
        Refuse(Mutation("fixture_create"), "fixture ownership mismatch");
        manager.DeleteMessageBox(unrelated);
        const auto releasedToken = Create();
        manager.GetMessageBoxes().front()->Release();
        CHECK(Inspect(releasedToken)["token_valid"] == false);
        Refuse(Mutation("fixture_retire", releasedToken), "fixture ownership mismatch");
        manager.PopMessageBox();
    }
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
}

TEST_CASE(
    "Native fixture identity dies through all native deletion routes and same address reuse [network][control-ui]")
{
    SEASON3B::NativeAssetBoundary assets;
    SEASON3B::MessageBoxLifecycleFixture storage;
    auto& manager = storage.manager;
    using Fixture = SEASON3B::ControlModalMessageBox;
    auto token = Fixture::CreateOwned(manager, "life");
    REQUIRE_FALSE(token.empty());
    auto* pointer = dynamic_cast<Fixture*>(manager.GetMessageBoxes().front());
    REQUIRE(pointer != nullptr);
    pointer->~Fixture();
    new (pointer) Fixture; // Exact same allocation, normal constructor/destructor.
    CHECK_FALSE(Fixture::Owns(manager, token));
    CHECK_FALSE(Fixture::Retire(manager, token));
    manager.DeleteMessageBox(pointer);
    auto next = Fixture::CreateOwned(manager, "life");
    REQUIRE_FALSE(next.empty());
    CHECK(next != token);
    manager.GetMessageBoxes().front()->Create(0, 0, 1, 1);
    CHECK_FALSE(Fixture::Owns(manager, next));
    manager.PopMessageBox();
    CHECK_FALSE(Fixture::Owns(manager, next));
    token = Fixture::CreateOwned(manager, "life");
    REQUIRE_FALSE(token.empty());
    manager.PopAllMessageBoxes();
    CHECK_FALSE(Fixture::Owns(manager, token));
    token = Fixture::CreateOwned(manager, "life");
    REQUIRE_FALSE(token.empty());
    manager.SendEvent(manager.GetMessageBoxes().front(), MSGBOX_EVENT_DESTROY);
    manager.Update(); // Ordinary event deletion is unchanged, never used by retirement.
    CHECK_FALSE(Fixture::Owns(manager, token));
    CHECK(manager.IsEmpty());
    token = Fixture::CreateOwned(manager, "life");
    REQUIRE_FALSE(token.empty());
    manager.Release();
    CHECK_FALSE(Fixture::Owns(manager, token));
    CHECK_FALSE(manager.HasMessageBoxStorage());
}
