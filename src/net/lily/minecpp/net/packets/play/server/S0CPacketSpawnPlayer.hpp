#pragma once
#include <array>
#include "EntityMetadata.hpp"
struct S0CPacketSpawnPlayer {
    int32_t entityId{}; std::array<uint8_t, 16> uuid{}; glm::dvec3 position{};
    float yaw{}, pitch{}; int16_t currentItem{}; EntityMetadata metadata{};
    static S0CPacketSpawnPlayer deserialize(const PacketBuffer& buffer) {
        S0CPacketSpawnPlayer packet;
        packet.entityId = static_cast<int32_t>(buffer.readVarInt());
        buffer.readBytes(packet.uuid.data(), packet.uuid.size());
        const int32_t x = buffer.readInt();
        const int32_t y = buffer.readInt();
        const int32_t z = buffer.readInt();
        packet.position = glm::dvec3(x, y, z) / 32.0;
        packet.yaw = static_cast<int8_t>(buffer.readByte()) * 360.0f / 256.0f;
        packet.pitch = static_cast<int8_t>(buffer.readByte()) * 360.0f / 256.0f;
        packet.currentItem = EntityMetadataDetail::readShort(buffer);
        packet.metadata = EntityMetadata::deserialize(buffer);
        return packet;
    }
};
