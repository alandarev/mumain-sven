#pragma once

#include <span>
#include <string>
#include <string_view>

class CHARACTER;

namespace App::Control
{
inline constexpr int QuickPeerObservationVersion = 1;
// Read-only allocation-owner check; rejects the temporary photo-viewer alias.
[[nodiscard]] std::span<CHARACTER> WorldCharacterStorage();
// Caller supplies proven storage; never interprets a server key as an array index.
[[nodiscard]] int ResolveQuickPeer(std::span<CHARACTER> storage, const CHARACTER* hero, int key, std::string_view id);
[[nodiscard]] bool QuickPeerAllowed(CHARACTER& hero, const CHARACTER& peer);
[[nodiscard]] std::string QuickPeerObservation(int key, std::string_view id);
[[nodiscard]] std::string QuickPeerRefusal(std::string_view observation, bool show);
[[nodiscard]] std::string ApplyQuickPeer(int key, std::string_view id, bool show, std::string_view expectedObservation);
} // namespace App::Control
