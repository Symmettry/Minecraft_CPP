#pragma once
#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"

struct S1BPacketEntityAttach {
    int32_t entityId{};
    int32_t vehicleId{}; // -1 detaches
    static S1BPacketEntityAttach deserialize(const PacketBuffer& buffer) {
        S1BPacketEntityAttach packet;
        packet.entityId = buffer.readInt();
        packet.vehicleId = buffer.readInt();
        buffer.readByte(); // leash flag
        return packet;
    }
};
