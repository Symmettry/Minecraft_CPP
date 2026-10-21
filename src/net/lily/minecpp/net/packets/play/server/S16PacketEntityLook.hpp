#pragma once
#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"
struct S16PacketEntityLook {
    int32_t entityId{}; float yaw{}, pitch{}; bool onGround{};
    static S16PacketEntityLook deserialize(const PacketBuffer& buffer) {
        S16PacketEntityLook packet;
        packet.entityId = static_cast<int32_t>(buffer.readVarInt());
        packet.yaw = static_cast<int8_t>(buffer.readByte()) * 360.0f / 256.0f;
        packet.pitch = static_cast<int8_t>(buffer.readByte()) * 360.0f / 256.0f;
        packet.onGround = buffer.readBool();
        return packet;
    }
};
