#pragma once
#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"
struct S17PacketEntityLookMove {
    int32_t entityId{}; glm::dvec3 delta{}; float yaw{}, pitch{}; bool onGround{};
    static S17PacketEntityLookMove deserialize(const PacketBuffer& buffer) {
        S17PacketEntityLookMove packet;
        packet.entityId = static_cast<int32_t>(buffer.readVarInt());
        const int8_t x = static_cast<int8_t>(buffer.readByte());
        const int8_t y = static_cast<int8_t>(buffer.readByte());
        const int8_t z = static_cast<int8_t>(buffer.readByte());
        packet.delta = glm::dvec3(x, y, z) / 32.0;
        packet.yaw = static_cast<int8_t>(buffer.readByte()) * 360.0f / 256.0f;
        packet.pitch = static_cast<int8_t>(buffer.readByte()) * 360.0f / 256.0f;
        packet.onGround = buffer.readBool();
        return packet;
    }
};
