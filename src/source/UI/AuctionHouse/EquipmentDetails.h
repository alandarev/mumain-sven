#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace UI::AuctionHouse
{
// One short excellent option of a row, marked when the search asked for it.
struct OptionMark
{
    std::string label;
    // The option's bit in the item's excellent byte and in the search request's ExcellentMask.
    std::uint8_t bit = 0;
    bool matched = false;
};

// Every excellent option of an item by its short name, in the Option list's order. `itemSlots` is the item's
// slot bit (`custom-packets.md`, SlotMask), which decides whether a bit is an offensive or defensive option.
std::vector<OptionMark> ExcellentMarks(std::uint16_t itemSlots, std::uint8_t excellentFlags);
// Marks the options the search asked for (the request's ExcellentMask).
void MarkSearchedOptions(std::vector<OptionMark>& marks, std::uint8_t searchedMask);

// What the character brings to an item's requirements: the facts only, never a score.
struct EquipFacts
{
    // The item's RequireClass row: 0 not wearable, n wearable from the n-th class step on.
    std::array<std::uint8_t, 7> requireClass{};
    std::uint8_t baseClass = 0, classStep = 1;
    struct Stat
    {
        std::string_view name;
        int required = 0, current = 0;
    };
    std::vector<Stat> stats;
};

// Why the character cannot equip an item: the class, then every stat below its requirement
// ("Strength 419 · you have 300"). Empty when the facts show no reason.
std::vector<std::string> CannotEquipReasons(const EquipFacts& facts);
} // namespace UI::AuctionHouse
