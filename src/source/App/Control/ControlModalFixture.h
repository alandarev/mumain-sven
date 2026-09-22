#pragma once

#include <string>
#include <string_view>

namespace App::Control
{
class Request;
// Main-thread only. Read-only inspect; mutations require its exact full guard.
[[nodiscard]] std::string ModalFixture(const Request& request, std::string_view snapshot, bool worldReady, bool muted);
[[nodiscard]] bool IsModalFixtureMutation(const Request& request);
} // namespace App::Control
