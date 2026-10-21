#pragma once
#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"
struct S0DPacketCollectItem {
    int32_t collectedEntityId{}; int32_t collectorEntityId{};
    static S0DPacketCollectItem deserialize(const PacketBuffer& buffer) {
        S0DPacketCollectItem packet;
        packet.collectedEntityId = static_cast<int32_t>(buffer.readVarInt());
        packet.collectorEntityId = static_cast<int32_t>(buffer.readVarInt());
        return packet;
    }
};
