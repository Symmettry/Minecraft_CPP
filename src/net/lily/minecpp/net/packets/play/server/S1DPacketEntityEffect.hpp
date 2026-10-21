#pragma once
#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"

struct S1DPacketEntityEffect {
    int32_t entityId{};
    uint8_t effectId{};
    static S1DPacketEntityEffect deserialize(const PacketBuffer& buffer) {
        S1DPacketEntityEffect packet;
        packet.entityId = static_cast<int32_t>(buffer.readVarInt());
        packet.effectId = buffer.readByte();
        buffer.readByte();   // amplifier
        buffer.readVarInt(); // duration
        buffer.readByte();   // hide particles
        return packet;
    }
};
