#pragma once
#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"
#include "EntityMetadata.hpp"

struct S04PacketEntityEquipment {
    int32_t entityId{};
    int16_t slot{};
    NetworkItemStack stack;

    static S04PacketEntityEquipment deserialize(const PacketBuffer& buffer) {
        S04PacketEntityEquipment packet;
        packet.entityId = static_cast<int32_t>(buffer.readVarInt());
        packet.slot = static_cast<int16_t>(buffer.readUShort());
        packet.stack = EntityMetadataDetail::readItemStack(buffer);
        return packet;
    }
};
