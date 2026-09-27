#include "stdafx.h"
#include "Core/Input/SyntheticInput.h"

#include "Core/Input/KeyState.h"
#include "Core/Input/UiInputRouter.h"

#include <SDL3/SDL.h>
#include "UI/Scaling/UITransform.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <string>

// Legacy mouse readers share these globals with the physical event loop.
// Only input not consumed by the UI may change them.
extern int MouseX;
extern int MouseY;
extern float g_fWindowMouseX;
extern float g_fWindowMouseY;
extern bool MouseLButton;
extern bool MouseLButtonPush;
extern bool MouseLButtonPop;
extern bool MouseRButton;
extern bool MouseRButtonPush;
extern bool MouseRButtonPop;
extern int g_iMousePopPosition_x;
extern int g_iMousePopPosition_y;
extern int g_iNoMouseTime;
extern unsigned int WindowWidth;
extern unsigned int WindowHeight;

namespace
{
constexpr int ReferenceWidth = 640;
constexpr int ReferenceHeight = 480;

enum class Kind : std::uint8_t
{
    None,
    Key,
    Click,
    Text,
    Wheel,
    Drag,
};

// Frames of the sequence. A key is down for the first frame only, which is
// what the key-state scan turns into a press edge; a click holds one frame
// longer so a button that acts on release sees down, held, up.
enum class Stage : std::uint8_t
{
    Idle,
    Pressed,
    Held,
    Released,
};

struct Injection
{
    Kind kind = Kind::None;
    Stage stage = Stage::Idle;
    int virtualKey = 0;
    float windowX = 0.0f;
    float windowY = 0.0f;
    Core::Input::Synthetic::MouseButton button = Core::Input::Synthetic::MouseButton::Left;
    std::string text;
    bool enter = false;
    // Wheel: notches still to queue, and whether a motion precedes them.
    int notches = 0;
    bool positioned = false;
    // Drag: the path from the press (`from`) to the release (`to`).
    Core::Input::Synthetic::WindowPoint from;
    Core::Input::Synthetic::WindowPoint to;
    int steps = 0;
    int moved = 0;
    bool legacyDown = false;
    bool uiDown = false;
    SDL_Window* window = nullptr;
    SDL_WindowID windowId = 0;
    Core::Input::IUiInputConsumer* consumer = nullptr;
};

Injection g_injection;
Core::Input::Synthetic::EventDelivery g_delivery = nullptr;
SDL_Window* g_window = nullptr;
// Failed owners can overlap newer schedules; keep outcomes until each act retires.
std::map<std::uint64_t, Core::Input::Synthetic::DeliveryFailure> g_failures;

// Numbers the injections, so a command can recognise its own. Never reused:
// an act compares the value it was given when its injection was accepted.
std::uint64_t g_generation = 0;

struct NamedKey
{
    std::string_view name;
    int virtualKey;
};

// Every key the SDL key-state shim translates (`VkToScancode`), by the name
// a caller types.
constexpr NamedKey NamedKeys[] = {
    {"esc", VK_ESCAPE},     {"escape", VK_ESCAPE}, {"enter", VK_RETURN},
    {"return", VK_RETURN},  {"tab", VK_TAB},       {"space", VK_SPACE},
    {"backspace", VK_BACK}, {"home", VK_HOME},     {"end", VK_END},
    {"insert", VK_INSERT},  {"delete", VK_DELETE}, {"pageup", VK_PRIOR},
    {"pagedown", VK_NEXT},  {"up", VK_UP},         {"down", VK_DOWN},
    {"left", VK_LEFT},      {"right", VK_RIGHT},   {"printscreen", VK_SNAPSHOT},
    {"f1", VK_F1},          {"f2", VK_F2},         {"f3", VK_F3},
    {"f4", VK_F4},          {"f5", VK_F5},         {"f6", VK_F6},
    {"f7", VK_F7},          {"f8", VK_F8},         {"f9", VK_F9},
    {"f10", VK_F10},        {"f11", VK_F11},       {"f12", VK_F12},
};

constexpr std::string_view KeyNameList =
    "a-z, 0-9, esc, enter, tab, space, backspace, home, end, insert, delete, pageup, pagedown, "
    "up, down, left, right, printscreen, f1-f12";

std::string Lowercase(std::string_view text)
{
    std::string lowered(text);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lowered;
}

int VirtualKeyForButton(Core::Input::Synthetic::MouseButton button)
{
    return button == Core::Input::Synthetic::MouseButton::Left ? VK_LBUTTON : VK_RBUTTON;
}

void ApplyPointerPosition()
{
    g_fWindowMouseX = g_injection.windowX;
    g_fWindowMouseY = g_injection.windowY;
    const auto transform =
        UI::Scaling::ScreenOverlayTransform(static_cast<int>(WindowWidth), static_cast<int>(WindowHeight));
    MouseX = std::clamp(static_cast<int>(UI::Scaling::LogicalX(transform, g_injection.windowX)), 0, ReferenceWidth);
    MouseY = std::clamp(static_cast<int>(UI::Scaling::LogicalY(transform, g_injection.windowY)), 0, ReferenceHeight);
    g_iNoMouseTime = 0;
}

void ApplyButtonDown()
{
    if (g_injection.button == Core::Input::Synthetic::MouseButton::Left)
    {
        MouseLButtonPop = false;
        if (!MouseLButton)
        {
            // Set, never cleared: a physical press recorded in the same frame
            // must keep its edge (Winmain.cpp:994 does the same).
            MouseLButtonPush = true;
        }
        MouseLButton = true;
        Core::Input::RecordLeftMouseButtonPressEdge();
        return;
    }
    MouseRButtonPop = false;
    if (!MouseRButton)
    {
        MouseRButtonPush = true;
    }
    MouseRButton = true;
}

void ApplyButtonUp()
{
    if (g_injection.button == Core::Input::Synthetic::MouseButton::Left)
    {
        // Set, never cleared, like the press edge above (Winmain.cpp:1023):
        // assigning would take back a release another path recorded in this
        // same frame.
        if (MouseLButton)
        {
            MouseLButtonPop = true;
        }
        MouseLButton = false;
        g_iMousePopPosition_x = MouseX;
        g_iMousePopPosition_y = MouseY;
        return;
    }
    if (MouseRButton)
    {
        MouseRButtonPop = true;
    }
    MouseRButton = false;
}

// Takes an injected press back, for an injection that is dropped before its
// sequence ended. Unlike ApplyButtonUp this raises no release edge: the
// caller has already been told its command did not finish, so the click must
// not complete behind its back.
void RetractButton()
{
    if (g_injection.button == Core::Input::Synthetic::MouseButton::Left)
    {
        MouseLButton = false;
        MouseLButtonPush = false;
        MouseLButtonPop = false;
        Core::Input::ClearLeftMouseButtonPressEdge();
        return;
    }
    MouseRButton = false;
    MouseRButtonPush = false;
    MouseRButtonPop = false;
}

// The window and UI consumer the injection was scheduled for are still the
// ones input is delivered to.
bool TargetUnchanged()
{
    return g_delivery && g_window == g_injection.window &&
           (!g_injection.window || SDL_GetWindowFromID(g_injection.windowId) == g_window) &&
           Core::Input::ActiveUiInputConsumer() == g_injection.consumer;
}

bool Deliver(SDL_Event& event, bool& propagates)
{
    if (!TargetUnchanged())
        return false;
    const auto generation = g_generation;
    const auto owner = g_injection.consumer;
    const bool delivered = g_delivery(event, propagates);
    return delivered && generation == g_generation && g_injection.kind != Kind::None &&
           g_window == g_injection.window && Core::Input::ActiveUiInputConsumer() == owner;
}

bool DeliverKey(bool down, int virtualKey, bool& propagates)
{
    SDL_Event event{};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.scancode = Core::Input::VkToScancode(virtualKey);
    event.key.key = SDL_GetKeyFromScancode(event.key.scancode, SDL_KMOD_NONE, false);
    event.key.windowID = g_injection.windowId;
    return Deliver(event, propagates);
}

bool DeliverMotion()
{
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.windowID = g_injection.windowId;
    event.motion.x = g_injection.windowX;
    event.motion.y = g_injection.windowY;
    bool propagates = true;
    return Deliver(event, propagates);
}

bool DeliverButton(bool down, bool& propagates)
{
    SDL_Event event{};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = g_injection.windowId;
    event.button.button =
        g_injection.button == Core::Input::Synthetic::MouseButton::Left ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT;
    event.button.x = g_injection.windowX;
    event.button.y = g_injection.windowY;
    return Deliver(event, propagates);
}

// Queued, not delivered: the main loop feeds the motion to the UI and the
// legacy pointer exactly as `hover` does.
bool QueueMotion()
{
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.timestamp = SDL_GetTicksNS();
    event.motion.windowID = g_injection.windowId;
    event.motion.x = g_injection.windowX;
    event.motion.y = g_injection.windowY;
    return SDL_PushEvent(&event);
}

bool QueueWheelNotch()
{
    const int notch = g_injection.notches > 0 ? 1 : -1;
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.timestamp = SDL_GetTicksNS();
    event.wheel.windowID = g_injection.windowId;
    event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    event.wheel.y = static_cast<float>(notch);
    event.wheel.integer_y = notch;
    event.wheel.mouse_x = g_injection.windowX;
    event.wheel.mouse_y = g_injection.windowY;
    if (!SDL_PushEvent(&event))
        return false;
    g_injection.notches -= notch;
    return true;
}

void Fail(Core::Input::Synthetic::DeliveryFailure reason)
{
    g_failures.emplace(g_generation, reason);
    Core::Input::Synthetic::Reset();
}

void BeginInjection(Kind kind)
{
    g_injection = {};
    ++g_generation;
    g_injection.kind = kind;
    g_injection.window = g_window;
    g_injection.windowId = g_window ? SDL_GetWindowID(g_window) : 0;
    g_injection.consumer = Core::Input::ActiveUiInputConsumer();
}

} // namespace

namespace Core::Input::Synthetic
{
std::optional<int> VirtualKeyFromName(std::string_view name)
{
    const std::string lowered = Lowercase(name);
    if (lowered.size() == 1)
    {
        const char c = lowered[0];
        if (c >= 'a' && c <= 'z')
        {
            return static_cast<int>(std::toupper(static_cast<unsigned char>(c)));
        }
        if (c >= '0' && c <= '9')
        {
            return static_cast<int>(c);
        }
        return std::nullopt;
    }

    for (const NamedKey& key : NamedKeys)
    {
        if (key.name == lowered)
        {
            return key.virtualKey;
        }
    }
    return std::nullopt;
}

std::string_view KeyNames()
{
    return KeyNameList;
}

std::optional<MouseButton> MouseButtonFromName(std::string_view name)
{
    const std::string lowered = Lowercase(name);
    if (lowered == "left")
    {
        return MouseButton::Left;
    }
    if (lowered == "right")
    {
        return MouseButton::Right;
    }
    return std::nullopt;
}

bool PressKey(int virtualKey)
{
    if (!IsIdle())
    {
        return false;
    }
    BeginInjection(Kind::Key);
    g_injection.virtualKey = virtualKey;
    return true;
}

bool Click(float windowX, float windowY, MouseButton button)
{
    if (!IsIdle())
    {
        return false;
    }
    BeginInjection(Kind::Click);
    g_injection.windowX = windowX;
    g_injection.windowY = windowY;
    g_injection.button = button;
    return true;
}

bool ValidText(std::string_view text)
{
    if (text.empty() || text.size() > 256)
        return false;
    for (std::size_t i = 0; i < text.size();)
    {
        const auto lead = static_cast<unsigned char>(text[i]);
        if (lead < 0x20 || lead == 0x7f)
            return false;
        if (lead < 0x80)
        {
            ++i;
            continue;
        }
        const int length = lead >= 0xc2 && lead <= 0xdf   ? 2
                           : lead >= 0xe0 && lead <= 0xef ? 3
                           : lead >= 0xf0 && lead <= 0xf4 ? 4
                                                          : 0;
        if (!length || i + length > text.size())
            return false;
        const auto second = static_cast<unsigned char>(text[i + 1]);
        if (second < 0x80 || second > 0xbf || (lead == 0xe0 && second < 0xa0) || (lead == 0xed && second > 0x9f) ||
            (lead == 0xf0 && second < 0x90) || (lead == 0xf4 && second > 0x8f))
            return false;
        for (int j = 2; j < length; ++j)
        {
            const auto continuation = static_cast<unsigned char>(text[i + j]);
            if (continuation < 0x80 || continuation > 0xbf)
                return false;
        }
        i += length;
    }
    return true;
}

bool TypeText(std::string_view text, bool enter)
{
    if (!ValidText(text) || !IsIdle())
        return false;
    BeginInjection(Kind::Text);
    g_injection.text = text;
    g_injection.enter = enter;
    return true;
}

bool ValidWheelNotches(int notches)
{
    return notches != 0 && notches >= -MaxWheelNotches && notches <= MaxWheelNotches;
}

bool Wheel(int notches, std::optional<WindowPoint> pointer)
{
    if (!ValidWheelNotches(notches) || !IsIdle())
        return false;
    BeginInjection(Kind::Wheel);
    g_injection.notches = notches;
    g_injection.positioned = pointer.has_value();
    g_injection.windowX = pointer ? pointer->x : g_fWindowMouseX;
    g_injection.windowY = pointer ? pointer->y : g_fWindowMouseY;
    return true;
}

bool ValidDragSteps(int steps)
{
    return steps >= MinDragSteps && steps <= MaxDragSteps;
}

bool Drag(WindowPoint from, WindowPoint to, MouseButton button, int steps)
{
    if (!ValidDragSteps(steps) || !IsIdle())
        return false;
    BeginInjection(Kind::Drag);
    g_injection.from = from;
    g_injection.to = to;
    g_injection.steps = steps;
    g_injection.windowX = from.x;
    g_injection.windowY = from.y;
    g_injection.button = button;
    return true;
}

void SetEventDelivery(EventDelivery delivery, SDL_Window* window)
{
    if ((g_delivery != delivery || g_window != window) && !IsIdle())
        Fail(DeliveryFailure::TargetLost);
    g_delivery = delivery;
    g_window = window;
}

DeliveryFailure FailureFor(std::uint64_t generation)
{
    const auto found = g_failures.find(generation);
    return found == g_failures.end() ? DeliveryFailure::None : found->second;
}

void ForgetOutcome(std::uint64_t generation)
{
    g_failures.erase(generation);
}

void CancelDelivery()
{
    if (!IsIdle())
        Fail(DeliveryFailure::TargetLost);
}

void CancelForPhysicalButton(unsigned char button)
{
    if ((g_injection.kind == Kind::Click || g_injection.kind == Kind::Drag) &&
        button == (g_injection.button == MouseButton::Left ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT))
        Fail(DeliveryFailure::PhysicalOverlap);
}

bool IsIdle()
{
    return g_injection.kind == Kind::None;
}

std::uint64_t CurrentGeneration()
{
    return g_generation;
}

bool IsKeyHeld(int virtualKey)
{
    const bool down =
        g_injection.legacyDown && (g_injection.stage == Stage::Pressed || g_injection.stage == Stage::Held);
    if (!down)
    {
        return false;
    }
    if (g_injection.kind == Kind::Key)
    {
        return virtualKey == g_injection.virtualKey;
    }
    const bool holdsButton = g_injection.kind == Kind::Click || g_injection.kind == Kind::Drag;
    return (holdsButton && virtualKey == VirtualKeyForButton(g_injection.button)) ||
           (g_injection.kind == Kind::Text && g_injection.stage == Stage::Held && virtualKey == VK_RETURN);
}

namespace
{
void AdvanceKey()
{
    bool propagates = true;
    if (g_injection.stage == Stage::Idle)
    {
        if (!DeliverKey(true, g_injection.virtualKey, propagates))
        {
            Fail(DeliveryFailure::TargetLost);
            return;
        }
        g_injection.legacyDown = propagates;
        g_injection.stage = Stage::Pressed;
    }
    else if (g_injection.stage == Stage::Pressed)
    {
        if (!DeliverKey(false, g_injection.virtualKey, propagates))
        {
            Fail(DeliveryFailure::TargetLost);
            return;
        }
        g_injection.legacyDown = false;
        g_injection.stage = Stage::Released;
    }
    else
        g_injection = {};
}

// Press, move and release shared by click and drag: the pointer arrives at the
// injection's position before the button changes, and only input the UI did
// not consume reaches the legacy mouse globals.
void PressAtPointer()
{
    bool propagates = true;
    const unsigned char button = g_injection.button == MouseButton::Left ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT;
    if ((SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_MASK(button)) != 0)
    {
        Fail(DeliveryFailure::PhysicalOverlap);
        return;
    }
    if (!DeliverMotion() || !DeliverButton(true, propagates))
    {
        Fail(DeliveryFailure::TargetLost);
        return;
    }
    g_injection.uiDown = !propagates;
    g_injection.legacyDown = propagates;
    if (propagates)
    {
        ApplyPointerPosition();
        ApplyButtonDown();
    }
    g_injection.stage = Stage::Pressed;
}

void MoveWhilePressed()
{
    if (!DeliverMotion())
    {
        Fail(DeliveryFailure::TargetLost);
        return;
    }
    if (g_injection.legacyDown)
        ApplyPointerPosition();
    g_injection.stage = Stage::Held;
}

void ReleaseAtPointer()
{
    bool propagates = true;
    if (!DeliverMotion() || !DeliverButton(false, propagates))
    {
        Fail(DeliveryFailure::TargetLost);
        return;
    }
    if (g_injection.legacyDown)
    {
        ApplyPointerPosition();
        ApplyButtonUp();
    }
    g_injection.legacyDown = false;
    g_injection.uiDown = false;
    g_injection.stage = Stage::Released;
}

void AdvanceClick()
{
    if (g_injection.stage == Stage::Idle)
        PressAtPointer();
    else if (g_injection.stage == Stage::Pressed)
        MoveWhilePressed();
    else if (g_injection.stage == Stage::Held)
        ReleaseAtPointer();
    else
        g_injection = {};
}

// One step along the straight path; the last step lands exactly on `to`.
void StepDragPointer()
{
    ++g_injection.moved;
    if (g_injection.moved >= g_injection.steps)
    {
        g_injection.windowX = g_injection.to.x;
        g_injection.windowY = g_injection.to.y;
        return;
    }
    const float progress = static_cast<float>(g_injection.moved) / static_cast<float>(g_injection.steps);
    g_injection.windowX = g_injection.from.x + (g_injection.to.x - g_injection.from.x) * progress;
    g_injection.windowY = g_injection.from.y + (g_injection.to.y - g_injection.from.y) * progress;
}

// Press at `from`, one move per frame for `steps` frames, release at `to`.
void AdvanceDrag()
{
    if (g_injection.stage == Stage::Idle)
        PressAtPointer();
    else if (g_injection.stage == Stage::Released)
        g_injection = {};
    else if (g_injection.moved < g_injection.steps)
    {
        StepDragPointer();
        MoveWhilePressed();
    }
    else
        ReleaseAtPointer();
}

// One queued notch per rendered frame: the legacy reader keeps only the last
// wheel event of a frame, so notches queued together would collapse into one.
// The frame after the last one is queued has consumed it.
void AdvanceWheel()
{
    if (g_injection.notches == 0)
    {
        g_injection = {};
        return;
    }
    const bool motionDue = g_injection.stage == Stage::Idle && g_injection.positioned;
    if (!TargetUnchanged() || (motionDue && !QueueMotion()) || !QueueWheelNotch())
    {
        Fail(DeliveryFailure::TargetLost);
        return;
    }
    g_injection.stage = Stage::Held;
}

void AdvanceText()
{
    bool propagates = true;
    if (g_injection.stage == Stage::Idle)
    {
        SDL_Event event{};
        event.type = SDL_EVENT_TEXT_INPUT;
        event.text.windowID = g_injection.windowId;
        event.text.text = g_injection.text.data();
        if (!Deliver(event, propagates))
        {
            Fail(DeliveryFailure::TargetLost);
            return;
        }
        g_injection.stage = Stage::Pressed;
    }
    else if (g_injection.stage == Stage::Pressed && g_injection.enter)
    {
        if (!DeliverKey(true, VK_RETURN, propagates))
        {
            Fail(DeliveryFailure::TargetLost);
            return;
        }
        g_injection.legacyDown = propagates;
        g_injection.stage = Stage::Held;
    }
    else if (g_injection.stage == Stage::Held)
    {
        if (!DeliverKey(false, VK_RETURN, propagates))
        {
            Fail(DeliveryFailure::TargetLost);
            return;
        }
        g_injection.legacyDown = false;
        g_injection.stage = Stage::Released;
    }
    else
        g_injection = {};
}

} // namespace

void BeginFrame()
{
    switch (g_injection.kind)
    {
    case Kind::Key:
        AdvanceKey();
        return;
    case Kind::Click:
        AdvanceClick();
        return;
    case Kind::Text:
        AdvanceText();
        return;
    case Kind::Wheel:
        AdvanceWheel();
        return;
    case Kind::Drag:
        AdvanceDrag();
        return;
    case Kind::None:
        return;
    }
}

void Reset()
{
    // A click or drag that is dropped mid-sequence has already written the button
    // down; take that back, or the game keeps seeing a button held by a frame
    // that will never come. A key needs nothing: `IsKeyHeld` reads the stage,
    // which goes away with the injection.
    const bool holdingButton =
        (g_injection.kind == Kind::Click || g_injection.kind == Kind::Drag) && g_injection.legacyDown;
    if (g_injection.uiDown)
        Core::Input::CancelSyntheticMousePress(
            g_injection.consumer, g_injection.button == MouseButton::Left ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT,
            g_injection.window);
    if (holdingButton)
    {
        RetractButton();
    }
    g_injection = {};
}
} // namespace Core::Input::Synthetic
