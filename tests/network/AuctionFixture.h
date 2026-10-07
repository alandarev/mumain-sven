#pragma once

#include "GameLogic/AuctionHouse/AuctionHousePacketFormat.h"
#include "doctest.h"

#include <filesystem>
#include <fstream>
#include <iterator>

namespace AuctionFixture
{
inline GameLogic::AuctionHouse::PacketBytes Load(std::string_view name)
{
    const auto path = std::filesystem::path(AUCTION_FIXTURE_DIR) / (std::string(name) + ".bin");
    std::ifstream stream(path, std::ios::binary);
    REQUIRE(stream.good());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

inline GameLogic::AuctionHouse::EntityId Id(std::uint8_t first)
{
    GameLogic::AuctionHouse::EntityId id{};
    for (std::size_t i = 0; i < id.size(); ++i) id[i] = static_cast<std::uint8_t>(first + i);
    return id;
}

template <typename T> T Decode(std::string_view name)
{
    auto parsed = GameLogic::AuctionHouse::ParseServerPacket(Load(name));
    REQUIRE(parsed.has_value());
    REQUIRE(std::holds_alternative<T>(*parsed));
    return std::get<T>(*parsed);
}
} // namespace AuctionFixture
