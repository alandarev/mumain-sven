#pragma once

#include "GameLogic/AuctionHouse/AuctionHouseTypes.h"

#include <optional>
#include <span>
#include <variant>

namespace GameLogic::AuctionHouse
{
inline constexpr std::uint8_t CustomHeadCode = 0x7B;
inline constexpr std::size_t MaximumNameMatches = 48;
inline constexpr std::size_t MaximumAncientMembers = 16;
inline constexpr std::size_t MaximumGoodsTypes = 64;

using PacketBytes = std::vector<std::uint8_t>;
using ServerPacket = std::variant<RulesUpdate, SearchPage, GoodsPage, Quote, SellCheck, OwnListingsPage, CollectionPage,
                                  Outcome, Notice>;

PacketBytes BuildSearchRequest(std::uint16_t id, const SearchFilter&, std::uint16_t page);
PacketBytes BuildGoodsListRequest(std::uint16_t id, std::span<const std::uint16_t> types, std::uint16_t page);
// Wire fields retain registry order; golden fixtures verify every argument.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
PacketBytes BuildQuoteRequest(std::uint16_t id, std::uint16_t type, std::uint8_t level, std::uint32_t quantity);
PacketBytes BuildBuyListingRequest(std::uint16_t id, const EntityId&, Zen reviewedPrice);
// Wire fields retain registry order; golden fixtures verify every argument.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
PacketBytes BuildBuyGoodsRequest(std::uint16_t id, std::uint16_t type, std::uint8_t level, std::uint32_t quantity,
                                 Zen reviewedTotal);
// Wire fields retain registry order; golden fixtures verify every argument.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
PacketBytes BuildCreateListingRequest(std::uint16_t id, std::uint8_t slot, std::uint32_t quantity, Zen unitPrice,
                                      std::uint8_t durationHours, Zen expectedPostingFee);
PacketBytes BuildCancelListingRequest(std::uint16_t id, const EntityId&);
PacketBytes BuildOwnListingsRequest(std::uint16_t id);
// Wire fields retain registry order; golden fixtures verify every argument.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
PacketBytes BuildCollectionRequest(std::uint16_t id, std::uint16_t page);
PacketBytes BuildClaimRequest(std::uint16_t id, const EntityId&);
// Wire fields retain registry order; golden fixtures verify every argument.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
PacketBytes BuildSellCheckRequest(std::uint16_t id, std::uint8_t slot);

bool IsValidSearchFilter(const SearchFilter&);
bool IsValidItemData(std::span<const std::uint8_t> bytes);
std::optional<ServerPacket> ParseServerPacket(std::span<const std::uint8_t> packet);
} // namespace GameLogic::AuctionHouse
