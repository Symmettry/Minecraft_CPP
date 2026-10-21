#pragma once
#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"
struct S18PacketEntityTeleport {
    int32_t entityId{}; glm::dvec3 position{}; float yaw{}, pitch{}; bool onGround{};
    static S18PacketEntityTeleport deserialize(const PacketBuffer& buffer) {
        S18PacketEntityTeleport packet;
        packet.entityId = static_cast<int32_t>(buffer.readVarInt());
        const int32_t x = buffer.readInt();
        const int32_t y = buffer.readInt();
        const int32_t z = buffer.readInt();
        packet.position = glm::dvec3(x, y, z) / 32.0;
        packet.yaw = static_cast<int8_t>(buffer.readByte()) * 360.0f / 256.0f;
        packet.pitch = static_cast<int8_t>(buffer.readByte()) * 360.0f / 256.0f;
        packet.onGround = buffer.readBool();
        return packet;
    }
};
