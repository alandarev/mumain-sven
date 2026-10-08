#pragma once

#include "GameLogic/AuctionHouse/AuctionHouseTypes.h"

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace GameLogic::AuctionHouse
{
// What makes two listings the same for pricing. Goods: type and level (loose jewels differ from
// bundles). Equipment: every option that changes its value, never its serial, slot or durability.
struct PricingIdentity
{
    bool goods = false;
    std::uint16_t type = 0;
    std::uint8_t level = 0;
    std::uint8_t excellentFlags = 0;
    bool luck = false, skill = false;
    std::uint8_t optionType = 0, optionLevel = 0;
    std::uint8_t ancientDiscriminator = 0, ancientBonus = 0;
    std::uint8_t socketCount = 0;
    std::array<std::uint8_t, 5> sockets{};
    std::uint16_t harmonyOption = 0, harmonyLevel = 0;
    bool option380 = false;

    bool operator==(const PricingIdentity&) const = default;
};

PricingIdentity GoodsPricingIdentity(std::uint16_t type, std::uint8_t level);

// The prices this account last published, per pricing identity, most recently used first: a
// publication and a prefill from the remembered price both count as a use (D5). One store belongs
// to one server and account; Load refuses a file written for another owner.
class PriceMemory
{
public:
    static constexpr std::size_t Capacity = 256;
    // Bounds against a damaged or hostile file: no line is read beyond this length, and no more
    // than Capacity entry lines are read (a repeated identity counts, its first line wins).
    static constexpr std::size_t MaximumLineLength = 1024;
    struct Entry
    {
        PricingIdentity identity;
        Zen price = 0;
    };

    std::optional<Zen> Find(const PricingIdentity& identity) const;
    // A prefill used the remembered price: it becomes the most recent entry. Memory only; the next
    // Save persists the order.
    std::optional<Zen> Use(const PricingIdentity& identity);
    void Remember(const PricingIdentity& identity, Zen price);
    bool Forget(const PricingIdentity& identity);
    void Clear();
    std::span<const Entry> Entries() const
    {
        return m_Entries;
    }
    bool Load(const std::filesystem::path& path, std::string_view owner);
    bool Save(const std::filesystem::path& path, std::string_view owner) const;

private:
    std::vector<Entry> m_Entries;
};

// The owner names the connect server, the selected game server and the account login, never a password.
std::string PriceMemoryOwner(std::string_view connectAddress, std::uint16_t connectPort, std::string_view serverName,
                             int serverIndex, std::string_view account);
std::filesystem::path PriceMemoryPath(std::string_view owner);
} // namespace GameLogic::AuctionHouse
