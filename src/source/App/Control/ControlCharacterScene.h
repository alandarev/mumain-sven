#pragma once

// Read-only report of the character-list scene, answered by `ui list` while
// the client is on that scene (the main-scene window registry is stale
// there). It only describes; nothing here opens, closes or focuses anything.

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace App::Control
{
inline constexpr int CharacterSceneObservationVersion = 1;
// CServerMsgWin keeps this many lines on both clients (SMW_MSG_LINE_MAX).
inline constexpr int CharacterSceneServerMessageLines = 5;

// What the live collector read; an empty optional means unavailable, which
// the report states as null rather than as a negative observation.
struct CharacterSceneFacts
{
    bool rosterReceived = false;
    int selectedHero = -1; // client index; -1 when nothing is selected
    int characterSlots = 0;
    bool serverMessageVisible = false;
    int serverMessageLines = -1; // outside 0..CharacterSceneServerMessageLines means unknown
    std::vector<std::string> serverMessageTexts;
    std::optional<bool> characterSelectVisible;
    std::optional<bool> characterMakeVisible;
    std::optional<bool> messageVisible;
    std::optional<bool> systemMenuVisible;
    std::optional<bool> optionVisible;
    std::optional<bool> genericConfirmVisible;
    std::optional<bool> genericMenuVisible;
    bool inputFocused = false;
    std::optional<bool> inputIdle;
    std::optional<std::array<float, 2>> windowPointer;
    std::optional<std::array<int, 2>> logicalPointer;
};

// The versioned JSON object for the facts; pure, so it is testable headless.
[[nodiscard]] std::string CharacterSceneObject(const CharacterSceneFacts& facts);

// Main thread, on the character-list scene only.
[[nodiscard]] CharacterSceneFacts ObserveCharacterScene();
} // namespace App::Control
