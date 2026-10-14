#pragma once

#include <span>
#include <cstdint>

void ReceiveBuyExtended(std::span<const std::uint8_t> buffer);
void ReceiveSell(const std::uint8_t* buffer);
void ReceiveRepair(const std::uint8_t* buffer);
