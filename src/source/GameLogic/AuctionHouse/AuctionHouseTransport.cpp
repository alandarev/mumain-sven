#include "stdafx.h"
#include "GameLogic/AuctionHouse/AuctionHouseTransport.h"
#include "Network/Server/WSclient.h"
#include "Dotnet/Connection.h"
#include "Dotnet/PacketFunctions_ClientToServer.h"

namespace GameLogic::AuctionHouse::Transport
{
bool Send(std::span<const std::uint8_t> packet)
{
    return SocketClient && SocketClient->IsConnected() && SocketClient->ToGameServer()->SendAuctionHousePacket(packet);
}

void CloseDialog()
{
    if (SocketClient && SocketClient->IsConnected())
        SocketClient->ToGameServer()->SendCloseNpcRequest();
}
} // namespace GameLogic::AuctionHouse::Transport
