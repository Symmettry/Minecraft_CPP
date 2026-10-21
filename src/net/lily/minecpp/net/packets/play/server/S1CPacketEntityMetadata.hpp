#pragma once
#include "EntityMetadata.hpp"
struct S1CPacketEntityMetadata {
    int32_t entityId{}; EntityMetadata metadata{};
    static S1CPacketEntityMetadata deserialize(const PacketBuffer& buffer) {
        S1CPacketEntityMetadata packet;
        packet.entityId = static_cast<int32_t>(buffer.readVarInt());
        packet.metadata = EntityMetadata::deserialize(buffer);
        return packet;
    }
};
