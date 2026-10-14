#include "ArmoryPacketFormat.h"
#include "json.hpp"
#include <cmath>
#include <set>

namespace Network::Server::Armory
{
namespace
{
using Json = nlohmann::json;
std::string String(const Json& value, const char* key, std::size_t maximum = 512)
{
    auto result = value.at(key).get<std::string>();
    if (result.size() > maximum)
        throw std::invalid_argument("Armory text limit");
    return result;
}
std::uint32_t Unsigned(const Json& value, const char* key)
{
    const auto& number = value.at(key);
    if (!number.is_number_integer() || number.get<double>() < 0 || number.get<double>() > UINT32_MAX)
        throw std::invalid_argument("Armory integer");
    return number.get<std::uint32_t>();
}
float Finite(const Json& value, const char* key)
{
    const float result = value.at(key).get<float>();
    if (!std::isfinite(result) || std::abs(result) > 1e9f)
        throw std::invalid_argument("Armory value");
    return result;
}
const Json& Array(const Json& value, const char* key, std::size_t maximum)
{
    const auto& result = value.at(key);
    if (!result.is_array() || result.size() > maximum)
        throw std::invalid_argument("Armory array limit");
    return result;
}
std::vector<UI::Character::ArmorySource> Sources(const Json& value)
{
    std::vector<UI::Character::ArmorySource> result;
    for (const auto& source : Array(value, "sources", 128))
    {
        const auto slot = source.at("slot").get<int>();
        if (slot < -1 || slot >= 12)
            throw std::invalid_argument("Armory equipped slot");
        result.push_back({slot, String(source, "name", 128), String(source, "operation", 32), Finite(source, "value"),
                          String(source, "stat", 128), String(source, "amount", 64), String(source, "group", 32)});
    }
    return result;
}
void Write(std::span<std::uint8_t> data, std::size_t offset, std::uint32_t value, int bytes)
{
    for (int i = 0; i < bytes; ++i)
        data[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
UI::Character::ArmorySkill Skill(const Json& value)
{
    UI::Character::ArmorySkill result;
    result.number = static_cast<int>(Unsigned(value, "number"));
    if (result.number > UINT16_MAX)
        throw std::invalid_argument("Armory skill id");
    result.name = String(value, "name", 128);
    result.output = String(value, "output", 64);
    result.kind = String(value, "kind", 128);
    result.costs = String(value, "costs");
    result.restriction = String(value, "restriction");
    for (const auto& line : Array(value, "calculation", 32))
    {
        auto text = line.get<std::string>();
        if (text.size() > 512)
            throw std::invalid_argument("Armory calculation limit");
        result.calculation.push_back(std::move(text));
    }
    return result;
}
} // namespace
std::array<std::uint8_t, RequestSize> Encode(const UI::Character::ArmoryRequest& request)
{
    std::array<std::uint8_t, RequestSize> bytes{};
    bytes[0] = 0xc1;
    bytes[1] = RequestSize;
    bytes[2] = Head;
    bytes[3] = RequestCode;
    bytes[4] = Version;
    bytes[5] = request.apply ? 1 : 0;
    Write(bytes, 6, request.requestId, 4);
    Write(bytes, 10, request.skill, 2);
    bytes[12] = static_cast<std::uint8_t>(request.target >> 8);
    bytes[13] = static_cast<std::uint8_t>(request.target);
    Write(bytes, 14, request.expectedPoints, 4);
    for (std::size_t i = 0; i < request.additions.size(); ++i)
    {
        Write(bytes, 18 + i * 2, request.additions[i], 2);
        Write(bytes, 28 + i * 4, request.bases[i], 4);
    }
    return bytes;
}
std::optional<UI::Character::ArmorySnapshot> Decode(std::span<const std::uint8_t> packet)
{
    constexpr std::size_t HeaderSize = 6;
    if (packet.size() < HeaderSize || packet.size() > MaximumPayload + HeaderSize || packet[0] != 0xc2 ||
        packet[3] != Head || packet[4] != ResponseCode || packet[5] != Version ||
        ((static_cast<std::size_t>(packet[1]) << 8) | packet[2]) != packet.size())
        return std::nullopt;
    try
    {
        auto data = Json::parse(packet.begin() + HeaderSize, packet.end());
        if (Unsigned(data, "version") != Version)
            return std::nullopt;
        UI::Character::ArmorySnapshot result;
        result.requestId = Unsigned(data, "requestId");
        if (result.requestId == 0)
            return std::nullopt;
        result.freePoints = Unsigned(data, "freePoints");
        result.character = String(data, "character", 40);
        result.status = String(data, "status");
        result.target = String(data, "target");
        result.skill = Skill(data.at("skill"));
        std::set<int> indices;
        for (const auto& attribute : Array(data, "attributes", 5))
        {
            const int index = static_cast<int>(Unsigned(attribute, "index"));
            if (index < 0 || index >= 5 || !indices.insert(index).second)
                return std::nullopt;
            result.attributes.push_back({index, String(attribute, "label", 48), Unsigned(attribute, "base"),
                                         Unsigned(attribute, "maximum"), Finite(attribute, "total"),
                                         Sources(attribute)});
        }
        if (!indices.contains(0) || !indices.contains(1) || !indices.contains(2) || !indices.contains(3))
            return std::nullopt;
        std::set<std::string> ids;
        for (const auto& row : Array(data, "rows", 128))
        {
            auto id = String(row, "id", 64);
            if (!ids.insert(id).second)
                return std::nullopt;
            result.rows.push_back({std::move(id), String(row, "category", 48), String(row, "label", 128),
                                   String(row, "value", 128), String(row, "explanation", 2048), Sources(row)});
        }
        return result;
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}
} // namespace Network::Server::Armory
