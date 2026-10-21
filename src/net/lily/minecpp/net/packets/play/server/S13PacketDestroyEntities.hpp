#pragma once
#include <vector>
#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"
struct S13PacketDestroyEntities {
    std::vector<int32_t> entityIds;
    static S13PacketDestroyEntities deserialize(const PacketBuffer& buffer) {
        S13PacketDestroyEntities packet;
        const uint32_t count = buffer.readVarInt();
        packet.entityIds.reserve(count);
        for (uint32_t index = 0; index < count; ++index)
            packet.entityIds.push_back(static_cast<int32_t>(buffer.readVarInt()));
        return packet;
    }
};
