#pragma once

#include "GameLogic/AuctionHouse/AuctionHousePacketFormat.h"

#include <algorithm>
#include <type_traits>

namespace GameLogic::AuctionHouse::Wire
{
inline constexpr std::uint8_t ShortHeader = 0xC1, LongHeader = 0xC2;
inline constexpr std::size_t ShortHeaderSize = 4, LongHeaderSize = 5;
inline constexpr unsigned BitsPerByte = 8;

class Writer
{
public:
    // Registry order is intentional and covered by every outgoing golden fixture.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    Writer(std::uint8_t subcode, std::uint16_t id) : bytes{ShortHeader, 0, CustomHeadCode, subcode}
    {
        bytes.reserve(255);
        Put(id);
    }

    template <typename T> void Put(T value)
    {
        static_assert(std::is_unsigned_v<T>);
        for (std::size_t i = 0; i < sizeof(T); ++i)
            bytes.push_back(static_cast<std::uint8_t>(value >> (i * BitsPerByte)));
    }

    void Put(const EntityId& id)
    {
        bytes.insert(bytes.end(), id.begin(), id.end());
    }
    PacketBytes Finish()
    {
        bytes[1] = static_cast<std::uint8_t>(bytes.size());
        return std::move(bytes);
    }

private:
    PacketBytes bytes;
};

class Reader
{
public:
    explicit Reader(std::span<const std::uint8_t> data) : bytes(data) {}

    template <typename T> T Get()
    {
        static_assert(std::is_unsigned_v<T>);
        const auto field = Take(sizeof(T));
        if (field.empty())
            return 0;
        T value = 0;
        for (std::size_t i = 0; i < sizeof(T); ++i)
            value |= static_cast<T>(static_cast<T>(field[i]) << (i * BitsPerByte));
        return value;
    }

    std::span<const std::uint8_t> Take(std::size_t count)
    {
        if (!valid || count > bytes.size())
        {
            valid = false;
            return {};
        }
        auto field = bytes.first(count);
        bytes = bytes.subspan(count);
        return field;
    }

    EntityId Id()
    {
        EntityId id{};
        const auto field = Take(id.size());
        std::copy(field.begin(), field.end(), id.begin());
        return id;
    }

    std::vector<std::uint8_t> Item(bool required)
    {
        const auto size = Get<std::uint8_t>();
        const auto field = Take(size);
        if ((required || size != 0) && !IsValidItemData(field))
            valid = false;
        return {field.begin(), field.end()};
    }

    Reply ReplyHeader()
    {
        Reply reply{Get<std::uint16_t>(), static_cast<Result>(Get<std::uint8_t>())};
        if (reply.requestId == 0 || reply.result > Result::PriceNotOnStep)
            valid = false;
        return reply;
    }

    bool Done() const
    {
        return valid && bytes.empty();
    }
    void Reject()
    {
        valid = false;
    }

private:
    std::span<const std::uint8_t> bytes;
    bool valid = true;
};

std::optional<ServerPacket> ParseRules(Reader&);
std::optional<ServerPacket> ParseSearch(Reader&);
std::optional<ServerPacket> ParseGoods(Reader&);
std::optional<ServerPacket> ParseQuote(Reader&);
std::optional<ServerPacket> ParseOwnListings(Reader&);
std::optional<ServerPacket> ParseCollection(Reader&);
std::optional<ServerPacket> ParseOutcome(Reader&, std::uint8_t subcode);
std::optional<ServerPacket> ParseNotice(Reader&);
std::optional<ServerPacket> ParseSellCheck(Reader&);

template <typename T> std::optional<ServerPacket> Finish(Reader& reader, T value)
{
    if (!reader.Done())
        return std::nullopt;
    return ServerPacket{std::move(value)};
}
} // namespace GameLogic::AuctionHouse::Wire
