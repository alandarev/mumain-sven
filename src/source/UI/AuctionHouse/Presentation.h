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
// Grouped digits; wide enough for a quantity times a unit price.
std::string ExactZen(std::uint64_t value);
std::string CompactZen(Market::Zen value);
// A balance in one compact form: thousands "k" or millions "kk" with at most two decimals, rounded down so it is
// never overstated (28,470,000 -> "28.47kk", 19,823,999 -> "19.82kk"); below 1,000 the exact amount.
std::string BalanceZen(Market::Zen value);
// A requirement after the item's harmony and socket reductions, as Cannot Equip counts it.
int EffectiveRequirement(int required, int reduction);
// The details line of one requirement: "Strength 370 (base 400)" when reduced, else "Strength 400" (UX3-04).
std::string RequirementText(std::string_view stat, int required, int reduction);
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
