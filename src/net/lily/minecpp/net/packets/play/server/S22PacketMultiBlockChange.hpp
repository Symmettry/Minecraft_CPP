#pragma once

#include <vector>

#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"
#include "net/lily/minecpp/world/chunk/Block.hpp"

class S22PacketMultiBlockChange final : public ClientBoundPacket {
public:
    struct BlockUpdateData {
        uint8_t x;
        uint8_t y;
        uint8_t z;
        Block block;
    };

    int32_t chunkX = 0;
    int32_t chunkZ = 0;
    std::vector<BlockUpdateData> updates;

    S22PacketMultiBlockChange() : ClientBoundPacket(0x22) {}

    static S22PacketMultiBlockChange deserialize(const PacketBuffer& buf) {
        S22PacketMultiBlockChange packet;
        packet.chunkX = buf.readInt();
        packet.chunkZ = buf.readInt();
        const uint32_t count = buf.readVarInt();
        packet.updates.reserve(count);
        for (uint32_t index = 0; index < count; ++index) {
            const uint16_t position = static_cast<uint16_t>(buf.readUShort());
            packet.updates.push_back({
                static_cast<uint8_t>(position >> 12 & 15),
                static_cast<uint8_t>(position & 255),
                static_cast<uint8_t>(position >> 8 & 15),
                static_cast<Block>(buf.readVarInt())
            });
        }
        return packet;
    }
};
