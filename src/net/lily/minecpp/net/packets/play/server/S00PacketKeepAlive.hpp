#pragma once
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

class S00PacketKeepAlive final : public ClientBoundPacket {
public:
    int key = 0;

    S00PacketKeepAlive() : ClientBoundPacket(0x00) {}

    static S00PacketKeepAlive deserialize(const PacketBuffer& buf) {
        S00PacketKeepAlive packet;
        packet.key = static_cast<int>(buf.readVarInt());
        return packet;
    }

};
