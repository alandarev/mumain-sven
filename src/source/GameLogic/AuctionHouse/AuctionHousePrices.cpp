#include "GameLogic/AuctionHouse/AuctionHouse.h"

namespace GameLogic::AuctionHouse
{
namespace
{
constexpr std::uint64_t DecimalBase = 10;

// The largest number of the form 1, 2 or 5 x 10^n that is at most price / 10^precision, at least 1.
// It matches the server's AuctionHouseFees.GetPriceStep, so both sides agree on every valid price.
std::uint64_t PriceStep(const Rules& rules, std::uint64_t price)
{
    if (rules.priceStepPrecision == 0)
        return 1;
    auto scaled = price;
    for (unsigned i = 0; i < rules.priceStepPrecision && scaled > 0; ++i)
        scaled /= DecimalBase;
    if (scaled < 2)
        return 1;
    std::uint64_t decade = 1;
    while (decade * DecimalBase <= scaled)
        decade *= DecimalBase;
    if (5 * decade <= scaled)
        return 5 * decade;
    if (2 * decade <= scaled)
        return 2 * decade;
    return decade;
}
} // namespace

bool PriceOnStep(const Rules& rules, Zen price)
{
    return price != 0 && price % PriceStep(rules, price) == 0;
}

std::optional<Zen> RoundPriceDown(const Rules& rules, Zen price)
{
    if (price == 0 || price > rules.maxPrice)
        return std::nullopt;
    const auto step = PriceStep(rules, price);
    return static_cast<Zen>(price / step * step);
}

std::optional<Zen> PreviousPrice(const Rules& rules, Zen price)
{
    if (price <= 1)
        return std::nullopt;
    // The step of the price just below decides, so a decade boundary steps down by the smaller step
    // (1,000,000 → 995,000 with precision 2).
    const std::uint64_t below = price - 1;
    const auto step = PriceStep(rules, below);
    const auto previous = below / step * step;
    if (previous == 0 || previous > rules.maxPrice)
        return std::nullopt;
    return static_cast<Zen>(previous);
}

std::optional<Zen> NextPrice(const Rules& rules, Zen price)
{
    if (price == 0 || price >= rules.maxPrice)
        return std::nullopt;
    const auto step = PriceStep(rules, price);
    const auto next = (price / step + 1) * step;
    if (next > rules.maxPrice)
        return std::nullopt;
    return static_cast<Zen>(next);
}

std::optional<Zen> StepPrice(const Rules& rules, Zen price, bool up, unsigned steps)
{
    std::optional<Zen> stepped;
    for (auto current = price; steps > 0; --steps)
    {
        const auto next = up ? NextPrice(rules, current) : PreviousPrice(rules, current);
        if (!next)
            break;
        current = *next;
        stepped = current;
    }
    return stepped;
}
} // namespace GameLogic::AuctionHouse
