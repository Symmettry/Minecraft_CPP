#pragma once

#include <vector>

#include "EntityMetadata.hpp"

struct S30PacketWindowItems {
    uint8_t windowId{};
    std::vector<NetworkItemStack> stacks;

    static S30PacketWindowItems deserialize(const PacketBuffer& buffer) {
        S30PacketWindowItems packet;
        packet.windowId = buffer.readByte();
        const uint16_t count = buffer.readUShort();
        packet.stacks.reserve(count);
        for (uint16_t slot = 0; slot < count; ++slot) {
            packet.stacks.push_back(EntityMetadataDetail::readItemStack(buffer));
        }
        return packet;
    }
};
