#pragma once
#include "UI/Character/Armory/ArmoryData.h"
#include <optional>
#include <span>

namespace Network::Server::Armory
{
void Expect(std::uint32_t requestId, std::string character);
void Receive(std::span<const std::uint8_t> packet);
std::optional<UI::Character::ArmorySnapshot> Take();
void Reset();
} // namespace Network::Server::Armory
