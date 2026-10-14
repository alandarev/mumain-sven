#include "doctest.h"
#include "Network/Server/ArmoryPacketFormat.h"
#include "Network/Server/ArmoryPackets.h"
#include "json.hpp"
using namespace Network::Server::Armory;
namespace
{
std::vector<std::uint8_t> Packet(std::uint32_t id = 1, std::string character = "Aren")
{
    nlohmann::json attributes = nlohmann::json::array();
    for (int i = 0; i < 4; ++i)
        attributes.push_back({{"index", i},
                              {"label", "Attribute"},
                              {"base", 100},
                              {"maximum", 65000},
                              {"total", 110},
                              {"sources", nlohmann::json::array()}});
    nlohmann::json data = {{"version", 1},
                           {"requestId", id},
                           {"character", character},
                           {"freePoints", 200},
                           {"status", ""},
                           {"attributes", attributes},
                           {"rows", nlohmann::json::array()},
                           {"target", "Select opponent"},
                           {"skill",
                            {{"number", 41},
                             {"name", "Death Stab"},
                             {"output", "100–200"},
                             {"kind", "Normal damage"},
                             {"costs", "10 Mana"},
                             {"restriction", ""},
                             {"calculation", nlohmann::json::array()}}}};
    const auto text = data.dump();
    std::vector<std::uint8_t> result{
        0xc2, static_cast<std::uint8_t>((text.size() + 6) >> 8), static_cast<std::uint8_t>(text.size() + 6), 0x7b, 0x51,
        1};
    result.insert(result.end(), text.begin(), text.end());
    return result;
}
} // namespace
TEST_CASE("Armory request matches the generated XML wire layout")
{
    UI::Character::ArmoryRequest request;
    request.requestId = 0x12345678;
    request.skill = 0x1234;
    request.target = 0x5678;
    request.apply = true;
    request.expectedPoints = 200;
    request.additions = {80, 60, 40, 20, 0};
    request.bases = {100, 80, 60, 40, 0};
    const auto bytes = Encode(request);
    const std::array<std::uint8_t, 48> golden{0xc1, 0x30, 0x7b, 0x50, 1,   1, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12,
                                              0x56, 0x78, 200,  0,    0,   0, 80,   0,    60,   0,    40,   0,
                                              20,   0,    0,    0,    100, 0, 0,    0,    80,   0,    0,    0,
                                              60,   0,    0,    0,    40,  0, 0,    0,    0,    0,    0,    0};
    CHECK(bytes == golden);
    CHECK(bytes.size() == 48);
    CHECK(bytes[0] == 0xc1);
    CHECK(bytes[1] == 48);
    CHECK(bytes[2] == 0x7b);
    CHECK(bytes[3] == 0x50);
    CHECK(bytes[5] == 1);
    CHECK(bytes[6] == 0x78);
    CHECK(bytes[9] == 0x12);
    CHECK(bytes[10] == 0x34);
    CHECK(bytes[11] == 0x12);
    CHECK(bytes[12] == 0x56);
    CHECK(bytes[13] == 0x78);
    CHECK(bytes[14] == 200);
    CHECK(bytes[18] == 80);
    CHECK(bytes[24] == 20);
    CHECK(bytes[28] == 100);
    CHECK(bytes[44] == 0);
}
TEST_CASE("Armory rejects malformed and unsupported responses without publishing partial data")
{
    auto packet = Packet();
    REQUIRE(Decode(packet));
    packet[5] = 2;
    CHECK_FALSE(Decode(packet));
    packet[5] = 1;
    packet.pop_back();
    CHECK_FALSE(Decode(packet));
    packet = Packet();
    packet[6] = 'x';
    CHECK_FALSE(Decode(packet));
    CHECK_FALSE(Decode(std::array<std::uint8_t, 6>{0xc2, 0, 6, 0x7b, 0x51, 1}));
}
TEST_CASE("Armory responses are correlated to both request and character")
{
    Reset();
    Expect(10, "Aren");
    Receive(Packet(9));
    CHECK_FALSE(Take());
    Receive(Packet(10, "Other"));
    CHECK_FALSE(Take());
    Receive(Packet(10));
    auto snapshot = Take();
    REQUIRE(snapshot);
    CHECK(snapshot->freePoints == 200);
    CHECK_FALSE(Take());
    Reset();
    Receive(Packet(10));
    CHECK_FALSE(Take());
}
