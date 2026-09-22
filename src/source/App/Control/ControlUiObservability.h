#pragma once

#include <string>

namespace App::Control
{
// Main-thread observation only. Null activity means unavailable, not inactive.
// This describes named mechanisms, not a guarantee that all input is safe.
[[nodiscard]] std::string UiObservabilityObject();
} // namespace App::Control
