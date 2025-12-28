#pragma once
#include <vector>
#include "../../Packet.hpp"
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

class S00PacketKeepAlive : public ClientBoundPacket {
public:
    int key = 0;

    S00PacketKeepAlive() : ClientBoundPacket(0x00) {}

    static S00PacketKeepAlive deserialize(const std::vector<uint8_t>& data) {
        S00PacketKeepAlive packet;
        size_t offset = 0;
        packet.key = static_cast<int>(readVarInt(data, offset));
        return packet;
    }

};
