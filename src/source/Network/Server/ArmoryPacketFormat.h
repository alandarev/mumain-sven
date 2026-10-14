#pragma once

#include "UI/Character/Armory/ArmoryData.h"
#include <optional>
#include <span>

namespace Network::Server::Armory
{
inline constexpr std::uint8_t Head = 0x7b, RequestCode = 0x50, ResponseCode = 0x51, Version = 1;
inline constexpr std::size_t RequestSize = 48, MaximumPayload = 48000;

// XML source: OpenMU Network/Packets/{ClientToServer,ServerToClient}/ArmoryPackets.xml.
std::array<std::uint8_t, RequestSize> Encode(const UI::Character::ArmoryRequest& request);
std::optional<UI::Character::ArmorySnapshot> Decode(std::span<const std::uint8_t> packet);
} // namespace Network::Server::Armory
