#pragma once
#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"
struct S19PacketEntityHeadLook {
    int32_t entityId{}; float yaw{};
    static S19PacketEntityHeadLook deserialize(const PacketBuffer& buffer) {
        S19PacketEntityHeadLook packet;
        packet.entityId = static_cast<int32_t>(buffer.readVarInt());
        packet.yaw = static_cast<int8_t>(buffer.readByte()) * 360.0f / 256.0f;
        return packet;
    }
};
