#pragma once

#if MU_ENABLE_CONTROL_SOCKET
#include <cstdint>
namespace App::Control
{
// Main-thread observations only. A boundary requires actual UI update and render,
// followed by renderer EndFrame; polling and skipped rendering never advance it.
void BeginUiFrame();
void ObserveUiUpdate();
void ObserveUiRender();
void CompleteUiFrame();
[[nodiscard]] std::uint64_t CompletedUiFrames();
} // namespace App::Control
#else
namespace App::Control
{
inline void BeginUiFrame() {}
inline void ObserveUiUpdate() {}
inline void ObserveUiRender() {}
inline void CompleteUiFrame() {}
} // namespace App::Control
#endif
