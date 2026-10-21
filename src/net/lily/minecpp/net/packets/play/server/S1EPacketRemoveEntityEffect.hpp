#pragma once
#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"

struct S1EPacketRemoveEntityEffect {
    int32_t entityId{};
    uint8_t effectId{};
    static S1EPacketRemoveEntityEffect deserialize(const PacketBuffer& buffer) {
        S1EPacketRemoveEntityEffect packet;
        packet.entityId = static_cast<int32_t>(buffer.readVarInt());
        packet.effectId = buffer.readByte();
        return packet;
    }
};
