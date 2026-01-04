#pragma once
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

class S05PacketSpawnPosition final : public ClientBoundPacket {
public:
    glm::vec<3, int> position;

    explicit S05PacketSpawnPosition() : ClientBoundPacket(0x05) {}

    static S05PacketSpawnPosition deserialize(const PacketBuffer& buf) {
        S05PacketSpawnPosition packet;
        packet.position = buf.readPosition();
        return packet;
    }

};
