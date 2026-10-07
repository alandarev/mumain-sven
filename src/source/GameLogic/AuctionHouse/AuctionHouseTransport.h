#pragma once

#include <cstdint>
#include <span>

namespace GameLogic::AuctionHouse::Transport
{
bool Send(std::span<const std::uint8_t> packet);
void CloseDialog();
} // namespace GameLogic::AuctionHouse::Transport
