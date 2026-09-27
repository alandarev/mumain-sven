// doctest unit tests for the scripted input injector: the key-name table and
// the frame sequence a `hotkey`, `click-ui`, `type`, `wheel` or `drag` walks
// through. The mouse globals it writes are plain variables and the events it
// queues are read back from SDL's queue, so no window is needed.
//
// Run: ctest --test-dir <build directory> --build-config Release -R "synthetic"

#include "doctest.h"

#include "Core/Input/KeyState.h"
#include "Core/Input/SyntheticInput.h"
#include "Core/Platform/WinCompat.h"
// After WinCompat.h, which supplies the Windows types it uses.
#include "Core/Globals/_define.h"

#ifdef MU_ENABLE_CONTROL_SOCKET
#include "App/Control/ControlCommands.h"
#endif

#include <SDL3/SDL.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

extern int MouseX;
extern int MouseY;
extern float g_fWindowMouseX;
extern float g_fWindowMouseY;
extern bool MouseLButton;
extern bool MouseLButtonPush;
extern bool MouseLButtonPop;
extern unsigned int WindowWidth;
extern unsigned int WindowHeight;
extern EGameScene SceneFlag;

using namespace Core::Input::Synthetic;

namespace
{
struct ResetInjector
{
    ResetInjector()
    {
        Reset();
    }
    ~ResetInjector()
    {
        Reset();
    }
};
} // namespace

TEST_CASE("Synthetic input names the keys the shim translates [core][synthetic-input]")
{
    CHECK(VirtualKeyFromName("esc") == VK_ESCAPE);
    CHECK(VirtualKeyFromName("Escape") == VK_ESCAPE);
    CHECK(VirtualKeyFromName("HOME") == VK_HOME);
    CHECK(VirtualKeyFromName("enter") == VK_RETURN);
    CHECK(VirtualKeyFromName("f1") == VK_F1);
    CHECK(VirtualKeyFromName("F12") == VK_F12);
    CHECK(VirtualKeyFromName("printscreen") == VK_SNAPSHOT);
    CHECK(VirtualKeyFromName("i") == 'I');
    CHECK(VirtualKeyFromName("I") == 'I');
    CHECK(VirtualKeyFromName("7") == '7');

    CHECK_FALSE(VirtualKeyFromName("bogus").has_value());
    CHECK_FALSE(VirtualKeyFromName("").has_value());
    CHECK_FALSE(VirtualKeyFromName("?").has_value());
    CHECK_FALSE(VirtualKeyFromName("f13").has_value());

    CHECK(MouseButtonFromName("left") == MouseButton::Left);
    CHECK(MouseButtonFromName("Right") == MouseButton::Right);
    CHECK_FALSE(MouseButtonFromName("middle").has_value());
}

TEST_CASE("A hotkey is down for exactly one frame [core][synthetic-input]")
{
    ResetInjector guard;
    CHECK(IsIdle());
    CHECK(PressKey(VK_HOME));
    CHECK_FALSE(IsIdle());

    // Scheduled, not yet applied: the frame has not begun.
    CHECK_FALSE(IsKeyHeld(VK_HOME));
    CHECK_FALSE(Core::Input::IsKeyDown(VK_HOME));

    BeginFrame();
    CHECK(IsKeyHeld(VK_HOME));
    CHECK(Core::Input::IsKeyDown(VK_HOME));
    CHECK_FALSE(IsKeyHeld(VK_END));

    BeginFrame();
    CHECK_FALSE(IsKeyHeld(VK_HOME));
    CHECK_FALSE(IsIdle());

    BeginFrame();
    CHECK(IsIdle());
}

TEST_CASE("A second injection is refused while one is in flight [core][synthetic-input]")
{
    ResetInjector guard;
    CHECK(PressKey(VK_ESCAPE));
    CHECK_FALSE(PressKey('I'));
    CHECK_FALSE(Click(1.0f, 1.0f, MouseButton::Left));

    BeginFrame();
    BeginFrame();
    BeginFrame();
    CHECK(IsIdle());
    CHECK(Click(1.0f, 1.0f, MouseButton::Left));
}

TEST_CASE("A click walks press, hold, release through the mouse globals [core][synthetic-input]")
{
    ResetInjector guard;
    WindowWidth = 1280;
    WindowHeight = 960;
    MouseLButton = false;
    MouseLButtonPush = false;
    MouseLButtonPop = false;

    CHECK(Click(1000.0f, 725.0f, MouseButton::Left));

    BeginFrame();
    CHECK(g_fWindowMouseX == doctest::Approx(1000.0f));
    CHECK(g_fWindowMouseY == doctest::Approx(725.0f));
    // The overlay space is 640x480 stretched over the window.
    CHECK(MouseX == 500);
    CHECK(MouseY == 362);
    CHECK(MouseLButton);
    CHECK(MouseLButtonPush);
    CHECK_FALSE(MouseLButtonPop);
    CHECK(IsKeyHeld(VK_LBUTTON));
    CHECK(Core::Input::IsKeyDown(VK_LBUTTON));

    // The scene clears the one-shot push at the end of the frame.
    MouseLButtonPush = false;
    BeginFrame();
    CHECK(MouseLButton);
    CHECK(IsKeyHeld(VK_LBUTTON));

    BeginFrame();
    CHECK_FALSE(MouseLButton);
    CHECK(MouseLButtonPop);
    CHECK_FALSE(IsKeyHeld(VK_LBUTTON));
    CHECK_FALSE(IsIdle());

    BeginFrame();
    CHECK(IsIdle());
}

namespace
{
std::vector<SDL_Event> QueuedEvents()
{
    std::vector<SDL_Event> events(16);
    const int count =
        SDL_PeepEvents(events.data(), static_cast<int>(events.size()), SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
    events.resize(count > 0 ? static_cast<std::size_t>(count) : 0);
    return events;
}

struct EventQueue
{
    EventQueue()
    {
        REQUIRE(SDL_InitSubSystem(SDL_INIT_EVENTS));
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    }
    ~EventQueue()
    {
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        SDL_QuitSubSystem(SDL_INIT_EVENTS);
    }
};
} // namespace

TEST_CASE("Typed text is queued, then Return is queued, held and let go [core][synthetic-input]")
{
    ResetInjector guard;
    EventQueue queue;
    CHECK_FALSE(TypeText("", false));
    REQUIRE(TypeText("hello", true));
    const auto generation = CurrentGeneration();
    CHECK_FALSE(PressKey(VK_HOME));

    BeginFrame();
    auto events = QueuedEvents();
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == SDL_EVENT_TEXT_INPUT);
    CHECK(std::string(events[0].text.text) == "hello");
    CHECK_FALSE(IsKeyHeld(VK_RETURN));

    BeginFrame();
    events = QueuedEvents();
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == SDL_EVENT_KEY_DOWN);
    CHECK(events[0].key.scancode == SDL_SCANCODE_RETURN);
    CHECK(events[0].key.key == SDLK_RETURN);
    CHECK_FALSE(IsKeyHeld(VK_RETURN));

    // The main loop has seen the key down; the key scan now sees it held.
    BeginFrame();
    CHECK(QueuedEvents().empty());
    CHECK(IsKeyHeld(VK_RETURN));
    CHECK(Core::Input::IsKeyDown(VK_RETURN));

    BeginFrame();
    events = QueuedEvents();
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == SDL_EVENT_KEY_UP);
    CHECK_FALSE(IsKeyHeld(VK_RETURN));
    CHECK(IsIdle());
    CHECK(CurrentGeneration() == generation);
    CHECK_FALSE(QueueFailed(generation));
}

TEST_CASE("Text without Return ends the frame after it is queued [core][synthetic-input]")
{
    ResetInjector guard;
    EventQueue queue;
    CHECK(ValidText(std::string(256, 'x')));
    CHECK_FALSE(ValidText(std::string(257, 'x')));
    CHECK(ValidText("\xc3\xa9"));
    CHECK_FALSE(ValidText("a\n"));
    CHECK_FALSE(ValidText("a\x7f"));
    CHECK_FALSE(ValidText("\xc0\xaf"));
    REQUIRE(TypeText("name", false));
    BeginFrame();
    CHECK(QueuedEvents().size() == 1);
    BeginFrame();
    CHECK(IsIdle());
    CHECK(QueuedEvents().empty());
}

TEST_CASE("A refused queue fails the injection instead of finishing it [core][synthetic-input]")
{
    ResetInjector guard;
    // No event queue: SDL refuses the event.
    REQUIRE(TypeText("hello", true));
    const auto generation = CurrentGeneration();
    BeginFrame();
    CHECK(IsIdle());
    CHECK(QueueFailed(generation));
    CHECK_FALSE(IsKeyHeld(VK_RETURN));
    REQUIRE(PressKey(VK_HOME));
    CHECK_FALSE(QueueFailed(CurrentGeneration()));
}

TEST_CASE("Wheel queues a positioned motion, then one notch per frame [core][synthetic-input]")
{
    ResetInjector guard;
    EventQueue queue;
    CHECK_FALSE(ValidWheelNotches(0));
    CHECK_FALSE(ValidWheelNotches(MaxWheelNotches + 1));
    CHECK(ValidWheelNotches(-MaxWheelNotches));
    CHECK_FALSE(Wheel(0, std::nullopt));

    REQUIRE(Wheel(-2, WindowPoint{30.0f, 40.0f}));
    CHECK_FALSE(Click(1.0f, 1.0f, MouseButton::Left));
    BeginFrame();
    auto events = QueuedEvents();
    REQUIRE(events.size() == 2);
    CHECK(events[0].type == SDL_EVENT_MOUSE_MOTION);
    CHECK(events[0].motion.x == 30.0f);
    CHECK(events[0].motion.y == 40.0f);
    CHECK(events[1].type == SDL_EVENT_MOUSE_WHEEL);
    CHECK(events[1].wheel.y == -1.0f);
    CHECK(events[1].wheel.integer_y == -1);
    CHECK(events[1].wheel.direction == SDL_MOUSEWHEEL_NORMAL);

    BeginFrame();
    events = QueuedEvents();
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == SDL_EVENT_MOUSE_WHEEL);
    CHECK_FALSE(IsIdle());

    BeginFrame();
    CHECK(QueuedEvents().empty());
    CHECK(IsIdle());
    CHECK_FALSE(IsKeyHeld(VK_LBUTTON));
}

TEST_CASE("A drag presses, moves one step per frame and releases at its end [core][synthetic-input]")
{
    ResetInjector guard;
    WindowWidth = 1280;
    WindowHeight = 960;
    MouseLButton = false;
    MouseLButtonPush = false;
    MouseLButtonPop = false;
    CHECK_FALSE(ValidDragSteps(MinDragSteps - 1));
    CHECK_FALSE(ValidDragSteps(MaxDragSteps + 1));
    CHECK_FALSE(Drag({100.0f, 200.0f}, {300.0f, 400.0f}, MouseButton::Left, 0));

    REQUIRE(Drag({100.0f, 200.0f}, {300.0f, 400.0f}, MouseButton::Left, 2));
    CHECK_FALSE(PressKey(VK_HOME));

    BeginFrame();
    CHECK(g_fWindowMouseX == 100.0f);
    CHECK(g_fWindowMouseY == 200.0f);
    CHECK(MouseLButton);
    CHECK(MouseLButtonPush);
    CHECK(IsKeyHeld(VK_LBUTTON));
    MouseLButtonPush = false;

    BeginFrame();
    CHECK(g_fWindowMouseX == 200.0f);
    CHECK(g_fWindowMouseY == 300.0f);
    CHECK(MouseX == 100);
    CHECK(MouseY == 150);
    CHECK(MouseLButton);

    BeginFrame();
    CHECK(g_fWindowMouseX == 300.0f);
    CHECK(g_fWindowMouseY == 400.0f);
    CHECK(MouseLButton);
    CHECK_FALSE(MouseLButtonPop);

    BeginFrame();
    CHECK_FALSE(MouseLButton);
    CHECK(MouseLButtonPop);
    CHECK_FALSE(IsKeyHeld(VK_LBUTTON));
    CHECK_FALSE(IsIdle());

    BeginFrame();
    CHECK(IsIdle());
}

TEST_CASE("An abandoned drag never leaves a button held [core][synthetic-input]")
{
    ResetInjector guard;
    WindowWidth = 1280;
    WindowHeight = 960;
    MouseLButton = false;
    MouseLButtonPop = false;
    REQUIRE(Drag({10.0f, 10.0f}, {50.0f, 50.0f}, MouseButton::Left, 4));
    BeginFrame();
    BeginFrame();
    REQUIRE(MouseLButton);
    Reset();
    CHECK(IsIdle());
    CHECK_FALSE(MouseLButton);
    CHECK_FALSE(MouseLButtonPush);
    CHECK_FALSE(MouseLButtonPop);
    CHECK_FALSE(IsKeyHeld(VK_LBUTTON));
    CHECK_FALSE(Core::Input::IsKeyDown(VK_LBUTTON));
}

#ifdef MU_ENABLE_CONTROL_SOCKET
namespace
{
std::string Respond(std::unique_ptr<App::Control::Act>& act)
{
    std::string response;
    CHECK(act->Tick(response) == App::Control::Act::Status::Finished);
    return response;
}
} // namespace

TEST_CASE("Type, wheel and drag handlers validate before scheduling [core][synthetic-input]")
{
    ResetInjector guard;
    WindowWidth = 800;
    WindowHeight = 600;
    std::unique_ptr<App::Control::Act> act;
    const auto generation = CurrentGeneration();
    for (const auto* raw : {R"({"cmd":"type"})", R"({"cmd":"type","text":""})",
                            R"({"cmd":"type","text":"x","enter":null})", R"({"cmd":"type","text":"x","enter":1})"})
    {
        CAPTURE(raw);
        const auto response = App::Control::Commands::Type(App::Control::Request::Parse(raw), act);
        CHECK(response.find(R"("error":"bad_request")") != std::string::npos);
        CHECK(act == nullptr);
    }
    for (const auto* raw : {R"({"cmd":"wheel"})", R"({"cmd":"wheel","notches":0})", R"({"cmd":"wheel","notches":6})",
                            R"({"cmd":"wheel","notches":-6})", R"({"cmd":"wheel","notches":1.5})",
                            R"({"cmd":"wheel","notches":1,"x":10})", R"({"cmd":"wheel","notches":1,"x":800,"y":10})",
                            R"({"cmd":"wheel","notches":1,"x":-1,"y":10})"})
    {
        CAPTURE(raw);
        const auto response = App::Control::Commands::Wheel(App::Control::Request::Parse(raw), act);
        CHECK(response.find(R"("error":"bad_request")") != std::string::npos);
        CHECK(act == nullptr);
    }
    for (const auto* raw :
         {R"({"cmd":"drag","from":[1,2]})", R"({"cmd":"drag","from":[1],"to":[3,4]})",
          R"({"cmd":"drag","from":[1,2],"to":[3,600]})", R"({"cmd":"drag","from":[1,2],"to":[3,4],"steps":0})",
          R"({"cmd":"drag","from":[1,2],"to":[3,4],"steps":31})",
          R"({"cmd":"drag","from":[1,2],"to":[3,4],"steps":2.5})",
          R"({"cmd":"drag","from":[1,2],"to":[3,4],"button":"middle"})"})
    {
        CAPTURE(raw);
        const auto response = App::Control::Commands::Drag(App::Control::Request::Parse(raw), act);
        CHECK(response.find(R"("error":"bad_request")") != std::string::npos);
        CHECK(act == nullptr);
    }
    CHECK(CurrentGeneration() == generation);

    REQUIRE(PressKey(VK_HOME));
    CHECK(App::Control::Commands::Type(App::Control::Request::Parse(R"({"cmd":"type","text":"x"})"), act)
              .find(R"("error":"busy")") != std::string::npos);
    CHECK(App::Control::Commands::Wheel(App::Control::Request::Parse(R"({"cmd":"wheel","notches":1})"), act)
              .find(R"("error":"busy")") != std::string::npos);
    CHECK(App::Control::Commands::Drag(App::Control::Request::Parse(R"({"cmd":"drag","from":[1,2],"to":[3,4]})"), act)
              .find(R"("error":"busy")") != std::string::npos);
    CHECK(act == nullptr);
}

TEST_CASE("Drag answers after its release with the final pointer [core][synthetic-input]")
{
    ResetInjector guard;
    WindowWidth = 800;
    WindowHeight = 600;
    std::unique_ptr<App::Control::Act> act;
    const auto request =
        App::Control::Request::Parse(R"({"cmd":"drag","id":5,"from":[10,20],"to":[110,220],"steps":1})");
    REQUIRE(App::Control::Commands::Drag(request, act).empty());
    REQUIRE(act != nullptr);
    CHECK_FALSE(act->IsAct());
    act->SetEncodedId(request.EncodedId());
    std::string response;
    for (int frame = 0; frame < 3; ++frame)
    {
        BeginFrame();
        CHECK(act->Tick(response) == App::Control::Act::Status::Running);
    }
    CHECK_FALSE(MouseLButton);
    BeginFrame();
    response = Respond(act);
    CHECK(response.find(R"("ok":true)") != std::string::npos);
    CHECK(response.find(R"("x":110.0)") != std::string::npos);
    CHECK(response.find(R"("y":220.0)") != std::string::npos);
    CHECK(response.find(R"("steps":1)") != std::string::npos);
}

TEST_CASE("A scene change abandons a drag and takes its press back [core][synthetic-input]")
{
    ResetInjector guard;
    WindowWidth = 800;
    WindowHeight = 600;
    const EGameScene scene = SceneFlag;
    SceneFlag = MAIN_SCENE;
    std::unique_ptr<App::Control::Act> act;
    REQUIRE(App::Control::Commands::Drag(
                App::Control::Request::Parse(R"({"cmd":"drag","from":[10,20],"to":[110,220]})"), act)
                .empty());
    REQUIRE(act != nullptr);
    BeginFrame();
    REQUIRE(MouseLButton);
    SceneFlag = CHARACTER_SCENE;
    const auto response = Respond(act);
    CHECK(response.find(R"("error":"failed")") != std::string::npos);
    CHECK(response.find("scene changed") != std::string::npos);
    CHECK(IsIdle());
    CHECK_FALSE(MouseLButton);
    SceneFlag = scene;
}

TEST_CASE("A timed-out drag's act drops its injection on destruction [core][synthetic-input]")
{
    ResetInjector guard;
    WindowWidth = 800;
    WindowHeight = 600;
    std::unique_ptr<App::Control::Act> act;
    REQUIRE(App::Control::Commands::Drag(
                App::Control::Request::Parse(R"({"cmd":"drag","from":[10,20],"to":[110,220]})"), act)
                .empty());
    BeginFrame();
    REQUIRE(MouseLButton);
    act.reset();
    CHECK(IsIdle());
    CHECK_FALSE(MouseLButton);
}
#endif
