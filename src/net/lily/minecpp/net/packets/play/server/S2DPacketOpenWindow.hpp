#pragma once

#include <cstdint>
#include <string>

#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"

struct S2DPacketOpenWindow {
    uint8_t windowId{};
    std::string inventoryType;
    std::string titleJson;
    uint8_t slotCount{};
    int32_t entityId = -1;

    static S2DPacketOpenWindow deserialize(const PacketBuffer& buffer) {
        S2DPacketOpenWindow packet;
        packet.windowId = buffer.readByte();
        packet.inventoryType = buffer.readString(32);
        packet.titleJson = buffer.readString(32767);
        packet.slotCount = buffer.readByte();
        if (packet.inventoryType == "EntityHorse") packet.entityId = buffer.readInt();
        return packet;
    }
};
