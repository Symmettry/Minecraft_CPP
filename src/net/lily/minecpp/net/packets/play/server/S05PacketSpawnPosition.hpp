#pragma once
#include <vector>
#include "../../Packet.hpp"
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

class S05PacketSpawnPosition : public ClientBoundPacket {
public:
    int spX, spY, spZ;

    explicit S05PacketSpawnPosition(const int spX, const int spY, const int spZ)
        : ClientBoundPacket(0x05), spX(spX), spY(spY), spZ(spZ) {}

    static S05PacketSpawnPosition deserialize(const std::vector<uint8_t>& data) {
        size_t offset = 0;
        S05PacketSpawnPosition packet{0,0,0}; // todo
        return packet;
    }

};
