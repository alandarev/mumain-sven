#pragma once

#include "GameLogic/AuctionHouse/AuctionHouseTypes.h"
#include "UI/AuctionHouse/RequestClock.h"

#include <chrono>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace UI::AuctionHouse
{
namespace Market = GameLogic::AuctionHouse;

// Decimal arithmetic only: an entered amount must resolve to an exact whole Zen.
std::optional<Market::Zen> ParseZen(std::string_view text, Market::Zen limit);
std::optional<std::uint32_t> ParseQuantity(std::string_view text, std::uint32_t limit);
std::string ExactZen(Market::Zen value);
std::string CompactZen(Market::Zen value);
std::string ResultText(Market::Result result);

enum class Category : std::uint8_t
{
    Equipment,
    Jewels,
    Materials
};
enum class Equipment : std::uint8_t
{
    All,
    Weapons,
    Armor,
    Wings,
    Accessories
};
struct Option
{
    std::string_view label;
    std::uint16_t slots;
    std::uint8_t mask;
};
std::span<const Option> Options();
// Option bits mean different things on weapon and armor items. Intersect the
// slot domain before sending the mask, so filtering applies to the entire market.
bool ApplyEquipmentFilter(Market::SearchFilter& filter, Equipment equipment, int option);
bool ParsePropertyQuery(Market::SearchFilter& filter, std::string& name);
bool MatchesGoodsName(std::string_view name, Category category, std::string_view query);
} // namespace UI::AuctionHouse
