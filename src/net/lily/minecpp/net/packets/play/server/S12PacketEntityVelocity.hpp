#pragma once
#include "EntityMetadata.hpp"
struct S12PacketEntityVelocity {
    int32_t entityId{}; glm::dvec3 velocity{};
    static S12PacketEntityVelocity deserialize(const PacketBuffer& buffer) {
        S12PacketEntityVelocity packet;
        packet.entityId = static_cast<int32_t>(buffer.readVarInt());
        const int16_t x = EntityMetadataDetail::readShort(buffer);
        const int16_t y = EntityMetadataDetail::readShort(buffer);
        const int16_t z = EntityMetadataDetail::readShort(buffer);
        packet.velocity = glm::dvec3(x, y, z) / 8000.0;
        return packet;
    }
};
