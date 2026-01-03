#pragma once
#include <vector>
#include <cstdint>
#include "S21PacketChunkData.hpp"
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

class S26PacketMapChunkBulk : public ClientBoundPacket {
public:
    std::vector<int> xPositions;
    std::vector<int> zPositions;
    std::vector<S21PacketChunkData::Chunk> chunksData;
    bool isOverworld = true;

    S26PacketMapChunkBulk() : ClientBoundPacket(0x26) {}

    static S26PacketMapChunkBulk deserialize(const std::vector<uint8_t>& buffer) {
        S26PacketMapChunkBulk packet;

        size_t offset = 0;

        packet.isOverworld = readBool(buffer, offset);

        const uint32_t count = readVarInt(buffer, offset);
        packet.xPositions.resize(count);
        packet.zPositions.resize(count);
        packet.chunksData.resize(count);

        for (uint32_t i = 0; i < count; ++i) {
            packet.xPositions[i] = readInt(buffer, offset);
            packet.zPositions[i] = readInt(buffer, offset);
            packet.chunksData[i].bitMask = readShort(buffer, offset);
        }

        for (uint32_t i = 0; i < count; ++i) {
            packet.chunksData[i] = S21PacketChunkData::deserializeChunk(packet.chunksData[i], true, packet.isOverworld, buffer, offset);
        }

        confirm(buffer, offset);

        return packet;
    }

    uint32_t getChunkCount() const {
        return xPositions.size();
    }

    const S21PacketChunkData::Chunk& getChunkData(const uint32_t index) const {
        return chunksData.at(index);
    }

    int getChunkX(const uint32_t index) const {
        return xPositions.at(index);
    }

    int getChunkZ(const uint32_t index) const {
        return zPositions.at(index);
    }
};