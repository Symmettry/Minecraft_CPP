#pragma once
#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"
struct S15PacketEntityRelMove {
    int32_t entityId{}; glm::dvec3 delta{}; bool onGround{};
    static S15PacketEntityRelMove deserialize(const PacketBuffer& buffer) {
        S15PacketEntityRelMove packet;
        packet.entityId = static_cast<int32_t>(buffer.readVarInt());
        const int8_t x = static_cast<int8_t>(buffer.readByte());
        const int8_t y = static_cast<int8_t>(buffer.readByte());
        const int8_t z = static_cast<int8_t>(buffer.readByte());
        packet.delta = glm::dvec3(x, y, z) / 32.0;
        packet.onGround = buffer.readBool();
        return packet;
    }
};
