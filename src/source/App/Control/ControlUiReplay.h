#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace App::Control
{
// ownsFixture is obtained synchronously from exact private config identity, never from wire JSON.
[[nodiscard]] std::string UiFixtureRefusal(std::string_view snapshot, bool ownsFixture);
// Empty means the snapshot meets the bounded starter-panel policy, not universal UI safety.
[[nodiscard]] std::string UiReplayRefusal(std::string_view snapshot, bool show, std::string_view window,
                                          bool allowSystemMenu);
// Calls the ordinary action only after checking the current main-thread snapshot.
[[nodiscard]] std::string ExecuteGuardedUi(std::string_view guard, std::string_view currentSnapshot, bool worldReady,
                                           bool muted, bool show, std::string_view window,
                                           const std::function<std::string()>& action);
} // namespace App::Control
