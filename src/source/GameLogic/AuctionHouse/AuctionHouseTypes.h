#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace GameLogic::AuctionHouse
{
using Zen = std::uint32_t;
using EntityId = std::array<std::uint8_t, 16>;
using ReceivedTime = std::chrono::steady_clock::time_point;

enum class Result : std::uint8_t
{
    Success,
    Unavailable,
    NotOpen,
    Busy,
    InvalidRequest,
    NotFound,
    QuoteChanged,
    InsufficientStock,
    InsufficientZen,
    NotEligible,
    ListingLimitReached,
    PriceOutOfRange,
    RulesChanged,
    OwnListing,
    Partial,
    InternalError,
    PriceNotOnStep
};

enum class Operation : std::uint8_t
{
    Search,
    GoodsList,
    Quote,
    BuyListing,
    BuyGoods,
    CreateListing,
    CancelListing,
    OwnListings,
    Collection,
    Claim,
    SellCheck,
    Count
};

struct Rules
{
    std::uint8_t pageSize = 0, maxActiveListings = 0, activeListings = 0;
    std::uint16_t pendingCollection = 0;
    Zen zenLimit = 0, maxPrice = 0, postingFeeFlat = 0;
    std::uint16_t postingFeeBasisPoints = 0, saleFeeBasisPoints = 0;
    bool refundPostingFee = false;
    std::vector<std::uint8_t> durations;
    std::uint8_t goodsPriceDigits = 0;
};

struct RulesUpdate
{
    bool open = false;
    Rules rules;
};

struct SearchEntry
{
    std::uint16_t type = 0;
    std::uint8_t discriminator = 0;
};

struct SearchFilter
{
    std::uint8_t sort = 0;
    std::uint16_t slotMask = 0, groupMask = 0;
    std::uint8_t classLine = 0xFF, flags = 0, family = 0;
    std::uint8_t upgradeMin = 0, upgradeMax = 15, excellentMask = 0;
    std::uint8_t minExcellentCount = 0, minSockets = 0;
    Zen maxPrice = 0;
    std::vector<SearchEntry> entries;
};

struct Reply
{
    std::uint16_t requestId = 0;
    Result result = Result::Success;
};

struct ListingRow
{
    EntityId id{};
    Zen price = 0;
    std::uint32_t secondsLeft = 0;
    std::string seller;
    bool requirementsMet = false, ownListing = false;
    std::vector<std::uint8_t> itemData;
};

struct SearchPage : Reply
{
    std::uint16_t page = 0;
    std::uint32_t totalCount = 0;
    std::vector<ListingRow> rows;
    ReceivedTime receivedAt{};
};

struct GoodsRow
{
    std::uint16_t type = 0;
    std::uint8_t level = 0;
    Zen lowestUnitPrice = 0;
    std::uint32_t available = 0;
    std::uint16_t listingCount = 0;
};

struct GoodsPage : Reply
{
    std::uint16_t page = 0;
    std::uint32_t totalCount = 0;
    std::vector<GoodsRow> rows;
    ReceivedTime receivedAt{};
};

struct PriceTier
{
    Zen unitPrice = 0;
    std::uint32_t quantity = 0;
};

struct Quote : Reply
{
    std::uint16_t type = 0;
    std::uint8_t level = 0;
    std::uint32_t requestedQuantity = 0, fillableQuantity = 0;
    Zen total = 0;
    std::vector<PriceTier> tiers;
    ReceivedTime receivedAt{};
};

struct SellCheck : Reply
{
    std::uint8_t inventorySlot = 0, reason = 0, kind = 0;
    std::uint16_t type = 0;
    std::uint8_t level = 0;
    std::uint32_t available = 0;
    Zen lowestUnitPrice = 0;
    std::uint8_t activeListings = 0;
};

struct OwnListing
{
    EntityId id{};
    Zen unitPrice = 0;
    std::uint32_t listed = 0, remaining = 0, secondsLeft = 0;
    std::uint8_t kind = 0;
    std::uint16_t type = 0;
    std::uint8_t level = 0;
    std::vector<std::uint8_t> itemData;
};

struct OwnListingsPage : Reply
{
    std::vector<OwnListing> rows;
    ReceivedTime receivedAt{};
};

struct CollectionEntry
{
    EntityId id{};
    std::uint8_t kind = 0;
    std::uint16_t type = 0;
    std::uint8_t level = 0;
    std::uint32_t quantity = 0;
    Zen gross = 0, fee = 0, net = 0;
    std::vector<std::uint8_t> itemData;
};

struct CollectionPage : Reply
{
    std::uint16_t page = 0, totalCount = 0;
    std::vector<CollectionEntry> rows;
    ReceivedTime receivedAt{};
};

struct Outcome : Reply
{
    Operation operation = Operation::Search;
    EntityId id{};
    std::uint16_t type = 0;
    std::uint8_t level = 0, delivery = 0;
    std::uint32_t quantity = 0, delivered = 0, kept = 0;
    Zen paid = 0, postingFee = 0, refundedFee = 0, zenCollected = 0;
    std::uint16_t completedEntries = 0, remainingEntries = 0;
    std::uint32_t itemsMoved = 0;
    std::uint8_t keptBack = 0;
};

enum class NoticeKind : std::uint8_t
{
    SoldToCollection = 1,
    ExpiredToCollection,
    DeliveriesWaiting,
    SoldToInventory,
    ExpiredToInventory
};

struct Notice
{
    NoticeKind kind{};
    std::uint16_t type = 0;
    std::uint8_t level = 0;
    std::uint32_t quantity = 0;
    Zen net = 0;
    std::uint16_t pendingCollection = 0;
};
} // namespace GameLogic::AuctionHouse
