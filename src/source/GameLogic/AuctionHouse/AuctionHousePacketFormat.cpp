#include "GameLogic/AuctionHouse/AuctionHouseWire.h"

#include <bit>

namespace GameLogic::AuctionHouse
{
bool IsValidSearchFilter(const SearchFilter& filter)
{
    if (filter.sort > 3 || (filter.slotMask & ~0x07FFu) != 0 || (filter.classLine > 6 && filter.classLine != 0xFF) ||
        filter.flags > 7 || filter.family > 4 || filter.upgradeMin > filter.upgradeMax || filter.upgradeMax > 15 ||
        filter.excellentMask > 0x3F || filter.minExcellentCount > 6 || filter.minSockets > 5)
        return false;

    std::size_t names = 0, ancient = 0;
    for (const auto& entry : filter.entries)
    {
        if (entry.type >= 16 * 512 || entry.discriminator > 2)
            return false;
        if (entry.discriminator == 0)
            ++names;
        else
            ++ancient;
    }
    return names <= MaximumNameMatches && ancient <= MaximumAncientMembers;
}

PacketBytes BuildSearchRequest(std::uint16_t id, const SearchFilter& filter, std::uint16_t page)
{
    if (id == 0 || !IsValidSearchFilter(filter))
        return {};
    Wire::Writer out(0x51, id);
    out.Put(page);
    out.Put(filter.sort);
    out.Put(filter.slotMask);
    out.Put(filter.groupMask);
    out.Put(filter.classLine);
    out.Put(filter.flags);
    out.Put(filter.family);
    out.Put(filter.upgradeMin);
    out.Put(filter.upgradeMax);
    out.Put(filter.excellentMask);
    out.Put(filter.minExcellentCount);
    out.Put(filter.minSockets);
    out.Put(filter.maxPrice);
    out.Put(static_cast<std::uint8_t>(filter.entries.size()));
    for (const auto& entry : filter.entries)
    {
        out.Put(entry.type);
        out.Put(entry.discriminator);
    }
    return out.Finish();
}

PacketBytes BuildGoodsListRequest(std::uint16_t id, std::span<const std::uint16_t> types, std::uint16_t page)
{
    if (id == 0 || types.size() > MaximumGoodsTypes ||
        std::any_of(types.begin(), types.end(), [](auto type) { return type >= 16 * 512; }))
        return {};
    Wire::Writer out(0x52, id);
    out.Put(page);
    out.Put(static_cast<std::uint8_t>(types.size()));
    for (auto type : types)
        out.Put(type);
    return out.Finish();
}

// Wire fields retain registry order; golden fixtures verify every argument.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
PacketBytes BuildQuoteRequest(std::uint16_t id, std::uint16_t type, std::uint8_t level, std::uint32_t quantity)
{
    Wire::Writer out(0x53, id);
    out.Put(type);
    out.Put(level);
    out.Put(quantity);
    return out.Finish();
}

PacketBytes BuildBuyListingRequest(std::uint16_t id, const EntityId& listing, Zen price)
{
    Wire::Writer out(0x54, id);
    out.Put(listing);
    out.Put(price);
    return out.Finish();
}

// Wire fields retain registry order; golden fixtures verify every argument.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
PacketBytes BuildBuyGoodsRequest(std::uint16_t id, std::uint16_t type, std::uint8_t level, std::uint32_t quantity,
                                 Zen total)
{
    Wire::Writer out(0x55, id);
    out.Put(type);
    out.Put(level);
    out.Put(quantity);
    out.Put(total);
    return out.Finish();
}

// Wire fields retain registry order; golden fixtures verify every argument.
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
PacketBytes BuildCreateListingRequest(std::uint16_t id, std::uint8_t slot, std::uint32_t quantity, Zen price,
                                      std::uint8_t duration, Zen fee)
{
    Wire::Writer out(0x56, id);
    out.Put(slot);
    out.Put(quantity);
    out.Put(price);
    out.Put(duration);
    out.Put(fee);
    return out.Finish();
}
// NOLINTEND(bugprone-easily-swappable-parameters)

PacketBytes BuildCancelListingRequest(std::uint16_t id, const EntityId& listing)
{
    Wire::Writer out(0x57, id);
    out.Put(listing);
    return out.Finish();
}

PacketBytes BuildOwnListingsRequest(std::uint16_t id)
{
    return Wire::Writer(0x58, id).Finish();
}

// Wire fields retain registry order; golden fixtures verify every argument.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
PacketBytes BuildCollectionRequest(std::uint16_t id, std::uint16_t page)
{
    Wire::Writer out(0x59, id);
    out.Put(page);
    return out.Finish();
}

PacketBytes BuildClaimRequest(std::uint16_t id, const EntityId& entry)
{
    Wire::Writer out(0x5A, id);
    out.Put(entry);
    return out.Finish();
}

// Wire fields retain registry order; golden fixtures verify every argument.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
PacketBytes BuildSellCheckRequest(std::uint16_t id, std::uint8_t slot)
{
    Wire::Writer out(0x5C, id);
    out.Put(slot);
    return out.Finish();
}

bool IsValidItemData(std::span<const std::uint8_t> bytes)
{
    constexpr std::size_t baseSize = 5;
    constexpr std::uint8_t variableOptionMask = 0x39, socketFlag = 0x80;
    if (bytes.size() < baseSize)
        return false;
    std::size_t length = baseSize + std::popcount(static_cast<unsigned>(bytes[4] & variableOptionMask));
    if ((bytes[4] & socketFlag) != 0)
    {
        if (bytes.size() <= length)
            return false;
        const auto count = bytes[length] & 0x0F;
        if (count > 5)
            return false;
        length += 1 + count;
    }
    return bytes.size() == length && bytes[2] <= 15 && (bytes[0] & 0x0F) <= 1;
}

std::optional<ServerPacket> ParseServerPacket(std::span<const std::uint8_t> packet)
{
    if (packet.size() < Wire::ShortHeaderSize)
        return std::nullopt;
    const bool longHeader = packet[0] == Wire::LongHeader;
    const auto headerSize = longHeader ? Wire::LongHeaderSize : Wire::ShortHeaderSize;
    if ((!longHeader && packet[0] != Wire::ShortHeader) || packet.size() < headerSize)
        return std::nullopt;
    const auto length = longHeader ? (std::size_t{packet[1]} << Wire::BitsPerByte) | packet[2] : packet[1];
    const auto subcode = packet[headerSize - 1];
    const bool list = subcode == 0x51 || subcode == 0x52 || subcode == 0x58 || subcode == 0x59;
    if (length != packet.size() || packet[headerSize - 2] != CustomHeadCode || list != longHeader)
        return std::nullopt;
    Wire::Reader in(packet.subspan(headerSize));
    switch (subcode)
    {
    case 0x50:
        return Wire::ParseRules(in);
    case 0x51:
        return Wire::ParseSearch(in);
    case 0x52:
        return Wire::ParseGoods(in);
    case 0x53:
        return Wire::ParseQuote(in);
    case 0x58:
        return Wire::ParseOwnListings(in);
    case 0x59:
        return Wire::ParseCollection(in);
    case 0x5B:
        return Wire::ParseNotice(in);
    case 0x5C:
        return Wire::ParseSellCheck(in);
    case 0x54:
    case 0x55:
    case 0x56:
    case 0x57:
    case 0x5A:
        return Wire::ParseOutcome(in, subcode);
    default:
        return std::nullopt;
    }
}
} // namespace GameLogic::AuctionHouse
