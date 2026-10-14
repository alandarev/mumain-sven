#include "ArmoryPackets.h"
#include "ArmoryPacketFormat.h"
#include <mutex>

namespace Network::Server::Armory
{
namespace
{
std::mutex Mutex;
std::uint32_t ExpectedId = 0;
std::string ExpectedCharacter;
std::optional<UI::Character::ArmorySnapshot> Response;
} // namespace
void Expect(std::uint32_t requestId, std::string character)
{
    const std::lock_guard lock(Mutex);
    ExpectedId = requestId;
    ExpectedCharacter = std::move(character);
    Response.reset();
}
void Receive(std::span<const std::uint8_t> packet)
{
    auto snapshot = Decode(packet);
    const std::lock_guard lock(Mutex);
    if (snapshot && snapshot->requestId == ExpectedId && snapshot->character == ExpectedCharacter)
        Response = std::move(snapshot);
}
std::optional<UI::Character::ArmorySnapshot> Take()
{
    const std::lock_guard lock(Mutex);
    auto result = std::move(Response);
    Response.reset();
    return result;
}
void Reset()
{
    Expect(0, {});
}
} // namespace Network::Server::Armory
