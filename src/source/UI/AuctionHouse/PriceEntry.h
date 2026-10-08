#pragma once

#include "GameLogic/AuctionHouse/AuctionHouseTypes.h"

#include <optional>
#include <string>
#include <string_view>

namespace UI::AuctionHouse
{
namespace Market = GameLogic::AuctionHouse;

inline constexpr unsigned ShiftPriceSteps = 10;
inline constexpr std::string_view PriceFormatHelp = "Use 590000, 590,000, 590k, 1.5kk or 1.2b (at most 2 decimals).";

enum class PriceSource : std::uint8_t
{
    None,
    Typed,
    Remembered,
    Lowest
};

struct PriceStepInput
{
    bool up = false;
    unsigned steps = 1;
};

struct PriceStepState
{
    bool down = false, up = false;
    std::string reason;
};

// The sell price field: what the player typed, where a prefilled value came from, and the
// commit, rounding and stepping rules. It never rounds while the player is still typing.
class PriceEntry
{
public:
    const std::string& Text() const
    {
        return m_Text;
    }
    PriceSource Source() const
    {
        return m_Source;
    }
    const std::string& Note() const
    {
        return m_Note;
    }
    bool Edited() const
    {
        return m_Edited;
    }
    void Reset();
    // The field changed. Returns false when the text is the entry's own last write.
    bool Edit(std::string_view text);
    // Enter, leaving the field, Review: shows the exact price, rounded down to a valid one.
    std::optional<Market::Zen> Commit(const Market::Rules& rules);
    // − / + and the keys; `lowest` is the lowest other-seller price, 0 when unknown or for equipment.
    bool Step(const Market::Rules& rules, PriceStepInput input, Market::Zen lowest);
    PriceStepState Steps(const Market::Rules& rules, Market::Zen lowest) const;
    // Match lowest and Undercut: the player's choice, committed at once.
    void Choose(const Market::Rules& rules, Market::Zen price);
    // A selected item's starting price: the unsaved draft, else the remembered price, else the
    // lowest other-seller price. Leaves an edited field alone and returns whether it wrote.
    bool Prefill(const Market::Rules& rules, std::optional<std::string_view> draft,
                 std::optional<Market::Zen> remembered, Market::Zen lowest);

private:
    void Write(Market::Zen price);

    std::string m_Text, m_Note;
    PriceSource m_Source = PriceSource::None;
    bool m_Edited = false;
};

// Help beside the price field for a first-time seller (D20), from the announced rules: shorthand
// examples that are valid prices ("590k · 1.5kk · 1.2b", never above MaxPrice or off a step), the grid and
// the keys ("Prices round down to ~1 % steps · ↑/↓ one step · Shift ten."), and the range.
struct PriceHelp
{
    std::string examples, grid, range;
};

PriceHelp DescribePriceHelp(const Market::Rules& rules);

std::string_view PriceSourceLabel(PriceSource source);
std::optional<Market::Zen> UndercutPrice(const Market::Rules& rules, Market::Zen lowest);
} // namespace UI::AuctionHouse
