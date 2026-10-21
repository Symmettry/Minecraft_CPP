#pragma once

#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"

struct S1APacketEntityStatus {
    int32_t entityId{};
    int8_t opcode{};

    static S1APacketEntityStatus deserialize(const PacketBuffer& buffer) {
        S1APacketEntityStatus packet;
        packet.entityId = buffer.readInt();
        packet.opcode = static_cast<int8_t>(buffer.readByte());
        return packet;
    }
};
