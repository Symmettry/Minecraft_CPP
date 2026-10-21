#pragma once

#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"
#include "net/lily/minecpp/world/chunk/Block.hpp"

class S23PacketBlockChange final : public ClientBoundPacket {
public:
    glm::ivec3 position{};
    Block block = BLOCK_AIR;

    S23PacketBlockChange() : ClientBoundPacket(0x23) {}

    static S23PacketBlockChange deserialize(const PacketBuffer& buf) {
        S23PacketBlockChange packet;
        packet.position = buf.readPosition();
        packet.block = static_cast<Block>(buf.readVarInt());
        return packet;
    }
};
