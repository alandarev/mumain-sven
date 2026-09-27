// Injected key presses and mouse clicks for scripted control of the client.
//
// An injection is applied below the game's own input readers — `IsKeyDown`
// consults the held key, a click or drag writes the same mouse globals the
// event loop fills from real SDL events — so every handler reacts exactly as
// it does to a human. Typed text, its Return and wheel notches are queued as
// SDL events instead, because the main loop's queue is the only feed of the
// portable text field and of `MouseWheel`. Nothing here moves the OS pointer
// or changes focus.
//
// Sequencing follows rendered frames: `BeginFrame()` runs once per rendered
// frame (before the key-state scan) and advances one injection through
// press -> hold -> release. At most one injection is in flight at a time.
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace Core::Input::Synthetic
{
enum class MouseButton : std::uint8_t
{
    Left,
    Right,
};

// Win32 virtual-key code for a key name (`esc`, `home`, `f1`, `a`, `7`, …),
// case-insensitive; empty when the name is unknown.
[[nodiscard]] std::optional<int> VirtualKeyFromName(std::string_view name);

// The key names `VirtualKeyFromName` accepts, for messages and usage.
[[nodiscard]] std::string_view KeyNames();

// Mouse button for `left` / `right`; empty for anything else.
[[nodiscard]] std::optional<MouseButton> MouseButtonFromName(std::string_view name);

// Schedules a one-frame press of the key. False when another injection is
// still in flight.
[[nodiscard]] bool PressKey(int virtualKey);

// Schedules a press-and-release of the button at a window pixel. False when
// another injection is still in flight.
[[nodiscard]] bool Click(float windowX, float windowY, MouseButton button);

// Committed UTF-8 text for the focused field, optionally followed by a
// separately framed Return.
[[nodiscard]] bool TypeText(std::string_view text, bool enter);
[[nodiscard]] bool ValidText(std::string_view text);

// A position in window pixels, as in a screenshot.
struct WindowPoint
{
    float x = 0.0f;
    float y = 0.0f;
};

// Wheel notches one request may scroll, in either direction (positive scrolls
// away from the user, like SDL's own wheel `y`).
inline constexpr int MaxWheelNotches = 5;
[[nodiscard]] bool ValidWheelNotches(int notches);

// Queues one SDL wheel event per rendered frame, so `MouseWheel` sees
// device-like notches; `pointer`, when given, is first queued as a motion like
// `hover`.
[[nodiscard]] bool Wheel(int notches, std::optional<WindowPoint> pointer);

// Frames a drag spends moving between its press and its release.
inline constexpr int MinDragSteps = 1;
inline constexpr int MaxDragSteps = 30;
inline constexpr int DefaultDragSteps = 8;
[[nodiscard]] bool ValidDragSteps(int steps);

// Presses at `from`, moves to `to` over `steps` rendered frames and releases
// at `to`, through the mouse globals like `Click`. Abandoning it (`Reset`)
// takes the press back without a release edge, so no button stays held.
[[nodiscard]] bool Drag(WindowPoint from, WindowPoint to, MouseButton button, int steps);

// True while no injection is in flight; the command that scheduled one
// answers once this turns true again.
[[nodiscard]] bool IsIdle();

// Identifies the injection most recently accepted: every schedule that
// returns true gets a value of its own, so a command can tell its own
// injection from the next caller's.
[[nodiscard]] std::uint64_t CurrentGeneration();

// Whether SDL refused an event the numbered injection queued; such an
// injection is dropped.
[[nodiscard]] bool QueueFailed(std::uint64_t generation);

// Whether the injected key is currently held; `IsKeyDown` ORs this in.
[[nodiscard]] bool IsKeyHeld(int virtualKey);

// Advances the in-flight injection by one rendered frame. Called once per
// rendered frame before the key-state scan.
void BeginFrame();

// Forgets any injection. A drag's press is taken back; a click, the only
// other held button, keeps its original behaviour of simply being forgotten.
// Called by the command that gives up on its own injection, and by the tests.
void Reset();
} // namespace Core::Input::Synthetic
