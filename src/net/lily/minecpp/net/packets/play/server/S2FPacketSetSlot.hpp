#pragma once

#include "EntityMetadata.hpp"

struct S2FPacketSetSlot {
    int8_t windowId{};
    int16_t slot{};
    NetworkItemStack stack{};

    static S2FPacketSetSlot deserialize(const PacketBuffer& buffer) {
        S2FPacketSetSlot packet;
        packet.windowId = static_cast<int8_t>(buffer.readByte());
        packet.slot = EntityMetadataDetail::readShort(buffer);
        packet.stack = EntityMetadataDetail::readItemStack(buffer);
        return packet;
    }
};
