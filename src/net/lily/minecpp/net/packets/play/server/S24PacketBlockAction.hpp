#pragma once

#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"
#include "net/lily/minecpp/world/chunk/Block.hpp"

class S24PacketBlockAction final : public ClientBoundPacket {
public:
    glm::ivec3 position{};
    uint8_t data1 = 0;
    uint8_t data2 = 0;
    Block block = BLOCK_AIR;

    S24PacketBlockAction() : ClientBoundPacket(0x24) {}

    static S24PacketBlockAction deserialize(const PacketBuffer& buf) {
        S24PacketBlockAction packet;
        packet.position = buf.readPosition();
        packet.data1 = buf.readByte();
        packet.data2 = buf.readByte();
        packet.block = static_cast<Block>(buf.readVarInt());
        return packet;
    }
};
