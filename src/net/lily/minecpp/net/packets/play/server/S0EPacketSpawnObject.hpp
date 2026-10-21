#pragma once
#include "EntityMetadata.hpp"
struct S0EPacketSpawnObject {
    int32_t entityId{}; uint8_t type{}; glm::dvec3 position{}; float yaw{}, pitch{};
    int32_t data{}; glm::dvec3 velocity{};
    static S0EPacketSpawnObject deserialize(const PacketBuffer& buffer) {
        S0EPacketSpawnObject packet;
        packet.entityId = static_cast<int32_t>(buffer.readVarInt());
        packet.type = buffer.readByte();
        const int32_t x = buffer.readInt();
        const int32_t y = buffer.readInt();
        const int32_t z = buffer.readInt();
        packet.position = glm::dvec3(x, y, z) / 32.0;
        packet.pitch = static_cast<int8_t>(buffer.readByte()) * 360.0f / 256.0f;
        packet.yaw = static_cast<int8_t>(buffer.readByte()) * 360.0f / 256.0f;
        packet.data = buffer.readInt();
        if (packet.data > 0) {
            const int16_t velocityX = EntityMetadataDetail::readShort(buffer);
            const int16_t velocityY = EntityMetadataDetail::readShort(buffer);
            const int16_t velocityZ = EntityMetadataDetail::readShort(buffer);
            packet.velocity = glm::dvec3(velocityX, velocityY, velocityZ) / 8000.0;
        }
        return packet;
    }
};
