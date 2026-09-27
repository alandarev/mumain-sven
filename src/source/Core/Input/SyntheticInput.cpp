#include "stdafx.h"
#include "Core/Input/SyntheticInput.h"

#include "Core/Input/KeyState.h"
#include "UI/Scaling/UITransform.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>

// Mouse state the event loop fills from real SDL events (ZzzOpenglUtil.cpp,
// Winmain.cpp); a click writes the same globals so the UI cannot tell the
// difference.
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
    SDL_WindowID windowId = 0;
    std::string text;
    // Text: whether a Return follows, and whether it is held this frame.
    bool enter = false;
    bool enterHeld = false;
    // Wheel: notches still to queue, and whether a motion precedes them.
    int notches = 0;
    bool positioned = false;
    // Drag: the path from the press (`from`) to the release (`to`).
    Core::Input::Synthetic::WindowPoint from;
    Core::Input::Synthetic::WindowPoint to;
    int steps = 0;
    int moved = 0;
};

Injection g_injection;

// Numbers the injections, so a command can recognise its own. Never reused.
std::uint64_t g_generation = 0;
// The injection whose queued event SDL refused, 0 for none.
std::uint64_t g_queueFailure = 0;
// SDL keeps only the pointer of an application's text event, so the text must
// outlive the queued event, which may outlive an abandoned injection.
std::string g_queuedText;

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
        MouseLButtonPush = !MouseLButton;
        MouseLButton = true;
        Core::Input::RecordLeftMouseButtonPressEdge();
        return;
    }
    MouseRButtonPop = false;
    MouseRButtonPush = !MouseRButton;
    MouseRButton = true;
}

void ApplyButtonUp()
{
    if (g_injection.button == Core::Input::Synthetic::MouseButton::Left)
    {
        MouseLButtonPop = MouseLButton;
        MouseLButton = false;
        g_iMousePopPosition_x = MouseX;
        g_iMousePopPosition_y = MouseY;
        return;
    }
    MouseRButtonPop = MouseRButton;
    MouseRButton = false;
}

// Takes an injected press back, for a drag that is dropped before it ended.
// Unlike ApplyButtonUp this raises no release edge: the caller has already
// been told its command did not finish, so nothing may be dropped behind its
// back.
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

// The game window events are addressed to (the client has one), 0 without.
SDL_WindowID GameWindowId()
{
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    const SDL_WindowID id = count > 0 ? SDL_GetWindowID(windows[0]) : 0;
    SDL_free(static_cast<void*>(windows));
    return id;
}

// Queued, not applied: the main loop feeds the motion to the pointer globals
// exactly as `hover` does.
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
    {
        return false;
    }
    g_injection.notches -= notch;
    return true;
}

bool QueueText()
{
    g_queuedText = g_injection.text;
    SDL_Event event{};
    event.type = SDL_EVENT_TEXT_INPUT;
    event.text.timestamp = SDL_GetTicksNS();
    event.text.windowID = g_injection.windowId;
    event.text.text = g_queuedText.c_str();
    return SDL_PushEvent(&event);
}

bool QueueReturn(bool down)
{
    SDL_Event event{};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.timestamp = SDL_GetTicksNS();
    event.key.windowID = g_injection.windowId;
    event.key.scancode = SDL_SCANCODE_RETURN;
    event.key.key = SDLK_RETURN;
    event.key.down = down;
    return SDL_PushEvent(&event);
}

void FailQueue()
{
    g_queueFailure = g_generation;
    g_injection = {};
}

void BeginInjection(Kind kind)
{
    g_injection = {};
    ++g_generation;
    g_injection.kind = kind;
}

void AdvanceKey()
{
    // Pressed -> Released: down for exactly one scan.
    g_injection.stage = g_injection.stage == Stage::Pressed ? Stage::Released : Stage::Idle;
    if (g_injection.stage == Stage::Idle)
    {
        g_injection.kind = Kind::None;
    }
}

void AdvanceClick()
{
    switch (g_injection.stage)
    {
    case Stage::Pressed:
        g_injection.stage = Stage::Held;
        return;
    case Stage::Held:
        ApplyPointerPosition();
        ApplyButtonUp();
        g_injection.stage = Stage::Released;
        return;
    case Stage::Released:
    case Stage::Idle:
        g_injection = {};
        return;
    }
}

// Text on the first frame; with Return, the key is queued on the second so
// the main loop has handled it (Enter gate, focused field) before the third
// frame's key scan sees it held, and is queued up on the fourth.
void AdvanceText()
{
    switch (g_injection.stage)
    {
    case Stage::Idle:
        if (!QueueText())
        {
            FailQueue();
            return;
        }
        g_injection.stage = Stage::Pressed;
        return;
    case Stage::Pressed:
        if (!g_injection.enter)
        {
            g_injection = {};
            return;
        }
        if (!QueueReturn(true))
        {
            FailQueue();
            return;
        }
        g_injection.stage = Stage::Held;
        return;
    case Stage::Held:
        g_injection.enterHeld = true;
        g_injection.stage = Stage::Released;
        return;
    case Stage::Released:
        g_injection.enterHeld = false;
        if (!QueueReturn(false))
        {
            FailQueue();
            return;
        }
        g_injection = {};
        return;
    }
}

// One queued notch per rendered frame: `MouseWheel` keeps only the last wheel
// event of a frame, so notches queued together would collapse into one. The
// frame after the last one is queued has consumed it.
void AdvanceWheel()
{
    if (g_injection.notches == 0)
    {
        g_injection = {};
        return;
    }
    const bool motionDue = g_injection.stage == Stage::Idle && g_injection.positioned;
    if ((motionDue && !QueueMotion()) || !QueueWheelNotch())
    {
        FailQueue();
        return;
    }
    g_injection.stage = Stage::Held;
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
    switch (g_injection.stage)
    {
    case Stage::Idle:
        ApplyPointerPosition();
        ApplyButtonDown();
        g_injection.stage = Stage::Pressed;
        return;
    case Stage::Pressed:
    case Stage::Held:
        if (g_injection.moved < g_injection.steps)
        {
            StepDragPointer();
            ApplyPointerPosition();
            g_injection.stage = Stage::Held;
            return;
        }
        ApplyPointerPosition();
        ApplyButtonUp();
        g_injection.stage = Stage::Released;
        return;
    case Stage::Released:
        g_injection = {};
        return;
    }
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
    g_injection.windowId = GameWindowId();
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
    g_injection.windowId = GameWindowId();
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

bool IsIdle()
{
    return g_injection.kind == Kind::None;
}

std::uint64_t CurrentGeneration()
{
    return g_generation;
}

bool QueueFailed(std::uint64_t generation)
{
    return generation != 0 && g_queueFailure == generation;
}

bool IsKeyHeld(int virtualKey)
{
    if (g_injection.kind == Kind::Text)
    {
        return g_injection.enterHeld && virtualKey == VK_RETURN;
    }
    const bool down = g_injection.stage == Stage::Pressed || g_injection.stage == Stage::Held;
    if (!down)
    {
        return false;
    }
    if (g_injection.kind == Kind::Key)
    {
        return virtualKey == g_injection.virtualKey;
    }
    const bool holdsButton = g_injection.kind == Kind::Click || g_injection.kind == Kind::Drag;
    return holdsButton && virtualKey == VirtualKeyForButton(g_injection.button);
}

void BeginFrame()
{
    switch (g_injection.kind)
    {
    case Kind::None:
        return;
    case Kind::Key:
        if (g_injection.stage == Stage::Idle)
        {
            g_injection.stage = Stage::Pressed;
            return;
        }
        AdvanceKey();
        return;
    case Kind::Click:
        if (g_injection.stage == Stage::Idle)
        {
            ApplyPointerPosition();
            ApplyButtonDown();
            g_injection.stage = Stage::Pressed;
            return;
        }
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
    }
}

void Reset()
{
    const bool holdingDrag =
        g_injection.kind == Kind::Drag && (g_injection.stage == Stage::Pressed || g_injection.stage == Stage::Held);
    if (holdingDrag)
    {
        RetractButton();
    }
    g_injection = {};
}
} // namespace Core::Input::Synthetic
