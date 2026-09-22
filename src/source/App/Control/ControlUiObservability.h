#pragma once

#include <optional>
#include <string>

namespace SEASON3B
{
class CNewUIManager;
class CNewUICryWolf;
class CNewUISiegeWarfare;
} // namespace SEASON3B

namespace App::Control
{
inline constexpr int UiObservabilityVersion = 3;

// Main-thread observation only. Null activity means unavailable, not inactive.
// This describes named mechanisms, not a guarantee that all input is safe.
[[nodiscard]] std::string UiObservabilityObject();

// Expected instances come from system ownership, not a registry type cast.
// These read-only seams permit headless tests without loading the whole UI.
[[nodiscard]] std::optional<bool> ObserveCryWolfEvent(SEASON3B::CNewUIManager* registry,
                                                      const SEASON3B::CNewUICryWolf* expected, bool worldReady);
[[nodiscard]] std::optional<bool> ObserveSiegeWarfareChild(SEASON3B::CNewUIManager* registry,
                                                           SEASON3B::CNewUISiegeWarfare* expected, bool worldReady);

} // namespace App::Control
