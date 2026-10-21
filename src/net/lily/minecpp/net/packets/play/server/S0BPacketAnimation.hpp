#pragma once
#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"
struct S0BPacketAnimation {
    int32_t entityId{}; uint8_t animation{};
    static S0BPacketAnimation deserialize(const PacketBuffer& buffer) {
        S0BPacketAnimation packet;
        packet.entityId = static_cast<int32_t>(buffer.readVarInt());
        packet.animation = buffer.readByte();
        return packet;
    }
};
