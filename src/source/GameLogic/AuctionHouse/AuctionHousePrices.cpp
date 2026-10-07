#include "GameLogic/AuctionHouse/AuctionHouse.h"

namespace GameLogic::AuctionHouse
{
namespace
{
constexpr Zen DecimalBase = 10;

std::uint64_t PriceStep(const Rules& rules, Zen price)
{
    if (rules.goodsPriceDigits == 0)
        return 1;
    unsigned digits = 1;
    for (auto remainder = price; remainder >= DecimalBase; remainder /= DecimalBase)
        ++digits;
    std::uint64_t step = 1;
    while (digits > rules.goodsPriceDigits)
    {
        step *= DecimalBase;
        --digits;
    }
    return step;
}
} // namespace

bool GoodsPriceOnStep(const Rules& rules, Zen price)
{
    return price != 0 && price % PriceStep(rules, price) == 0;
}

std::optional<Zen> PreviousGoodsPrice(const Rules& rules, Zen price)
{
    if (price <= 1)
        return std::nullopt;
    // Use the lower magnitude at a power-of-ten boundary (1,000,000 → 999,000).
    const auto below = price - 1;
    const auto step = PriceStep(rules, below);
    const auto previous = below / step * step;
    if (previous == 0 || previous > rules.maxPrice)
        return std::nullopt;
    return static_cast<Zen>(previous);
}

std::optional<Zen> NextGoodsPrice(const Rules& rules, Zen price)
{
    if (price == 0 || price >= rules.maxPrice)
        return std::nullopt;
    const auto step = PriceStep(rules, price);
    const auto next = (price / step + 1) * step;
    if (next > rules.maxPrice)
        return std::nullopt;
    return static_cast<Zen>(next);
}
} // namespace GameLogic::AuctionHouse
