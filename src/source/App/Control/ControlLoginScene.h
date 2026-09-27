#pragma once

// Read-only report of the login scene (server list and login form), answered
// by `ui list` while the client is on that scene (the main-scene window
// registry does not exist there). It only describes; nothing here opens,
// closes, focuses or submits anything, and of the password field it reports
// only whether it is empty.

#include <array>
#include <optional>
#include <string>

namespace App::Control
{
inline constexpr int LoginSceneObservationVersion = 1;

// What the live collector read; an empty optional means unavailable, which
// the report states as null rather than as a negative observation.
struct LoginSceneFacts
{
    int serverGroups = 0;        // groups in the connect server's list
    bool loginFormReady = false; // the game server accepted the join (RECEIVE_JOIN_SERVER_SUCCESS)
    std::optional<bool> serverSelectVisible;
    std::optional<bool> loginVisible;
    std::optional<bool> messageVisible;
    std::optional<bool> systemMenuVisible;
    std::optional<bool> optionVisible;
    std::optional<bool> creditVisible;
    std::optional<bool> genericConfirmVisible;
    std::optional<bool> genericMenuVisible;
    // The login form's account field text (a test account name, never a
    // secret) and whether the password field is empty; the password text
    // itself is never reported.
    std::optional<std::string> account;
    std::optional<bool> passwordEmpty;
    // "account", "password" or empty when neither login field has the focus.
    std::string focusedField;
    bool inputFocused = false;
    std::optional<bool> inputIdle;
    std::optional<std::array<float, 2>> windowPointer;
    std::optional<std::array<int, 2>> logicalPointer;
};

// The versioned JSON object for the facts; pure, so it is testable headless.
[[nodiscard]] std::string LoginSceneObject(const LoginSceneFacts& facts);

// Main thread, on the login scene only.
[[nodiscard]] LoginSceneFacts ObserveLoginScene();
} // namespace App::Control
