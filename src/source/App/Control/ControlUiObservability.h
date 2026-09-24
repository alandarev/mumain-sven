#pragma once

#include <optional>
#include <string>

#if __has_include("UI/Core/WindowSystem.h")
namespace mu::ui::window
{
class CManager;
class CCryWolf;
class CSiegeWarfare;
} // namespace mu::ui::window
#else
namespace SEASON3B
{
class CNewUIManager;
class CNewUICryWolf;
class CNewUISiegeWarfare;
} // namespace SEASON3B
#endif

namespace App::Control
{
inline constexpr int UiObservabilityVersion = 3;

// Main-thread observation only. Null activity means unavailable, not inactive.
// This describes named mechanisms, not a guarantee that all input is safe.
[[nodiscard]] std::string UiObservabilityObject();

// Expected instances come from system ownership, not a registry type cast.
// These read-only seams permit headless tests without loading the whole UI.
#if __has_include("UI/Core/WindowSystem.h")
[[nodiscard]] std::optional<bool> ObserveCryWolfEvent(mu::ui::window::CManager* registry,
                                                      const mu::ui::window::CCryWolf* expected, bool worldReady);
[[nodiscard]] std::optional<bool> ObserveSiegeWarfareChild(mu::ui::window::CManager* registry,
                                                           mu::ui::window::CSiegeWarfare* expected, bool worldReady);
#else
[[nodiscard]] std::optional<bool> ObserveCryWolfEvent(SEASON3B::CNewUIManager* registry,
                                                      const SEASON3B::CNewUICryWolf* expected, bool worldReady);
[[nodiscard]] std::optional<bool> ObserveSiegeWarfareChild(SEASON3B::CNewUIManager* registry,
                                                           SEASON3B::CNewUISiegeWarfare* expected, bool worldReady);
#endif
} // namespace App::Control
