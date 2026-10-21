#pragma once

#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

class S25PacketBlockBreakAnim final : public ClientBoundPacket {
public:
    int32_t breakerId = 0;
    glm::ivec3 position{};
    uint8_t progress = 0;

    S25PacketBlockBreakAnim() : ClientBoundPacket(0x25) {}

    static S25PacketBlockBreakAnim deserialize(const PacketBuffer& buf) {
        S25PacketBlockBreakAnim packet;
        packet.breakerId = static_cast<int32_t>(buf.readVarInt());
        packet.position = buf.readPosition();
        packet.progress = buf.readByte();
        return packet;
    }
};
