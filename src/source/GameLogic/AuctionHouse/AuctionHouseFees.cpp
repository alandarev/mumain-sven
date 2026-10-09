#include "GameLogic/AuctionHouse/AuctionHouse.h"

#include <algorithm>
#include <limits>

namespace GameLogic::AuctionHouse
{
namespace
{
constexpr std::uint64_t BasisPointDivisor = 10000;
}

std::optional<Zen> PostingFee(const Rules& rules, std::uint32_t quantity, Zen price)
{
    const auto total = std::uint64_t{quantity} * price;
    if (quantity == 0 || price == 0 || total > rules.maxPrice)
        return std::nullopt;
    const auto fee = rules.postingFeeFlat + total * rules.postingFeeBasisPoints / BasisPointDivisor;
    if (fee > rules.maxPrice)
        return std::nullopt;
    return static_cast<Zen>(fee);
}

Zen SaleFee(const Rules& rules, Zen gross)
{
    // Capped at the gross before narrowing, as the server does: a fee above 100 % takes the whole sale.
    const auto fee = std::uint64_t{gross} * rules.saleFeeBasisPoints / BasisPointDivisor;
    return static_cast<Zen>(std::min<std::uint64_t>(fee, gross));
}

std::uint32_t SecondsRemaining(std::uint32_t seconds, ReceivedTime receivedAt, ReceivedTime now)
{
    if (now <= receivedAt)
        return seconds;
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - receivedAt).count();
    return static_cast<std::uint64_t>(elapsed) >= seconds ? 0 : seconds - static_cast<std::uint32_t>(elapsed);
}
} // namespace GameLogic::AuctionHouse
