#pragma once
#include <vector>
#include <cstdint>
#include "S21PacketChunkData.hpp"
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

class S26PacketMapChunkBulk final : public ClientBoundPacket {
public:
    std::vector<int> xPositions;
    std::vector<int> zPositions;
    std::vector<S21PacketChunkData::Chunk> chunksData;
    bool skyLightSent = false;

    S26PacketMapChunkBulk() : ClientBoundPacket(0x26) {}

    static S26PacketMapChunkBulk deserialize(const PacketBuffer& buf) {
        S26PacketMapChunkBulk packet;

        packet.skyLightSent = buf.readBool();

        const uint32_t count = buf.readVarInt();
        packet.xPositions.resize(count);
        packet.zPositions.resize(count);
        packet.chunksData.resize(count);

        for (uint32_t i = 0; i < count; ++i) {
            packet.xPositions[i] = buf.readInt();
            packet.zPositions[i] = buf.readInt();
            packet.chunksData[i].bitMask = buf.readShort();
        }

        for (uint32_t i = 0; i < count; ++i) {
            S21PacketChunkData::deserializeChunk(packet.chunksData[i], true, packet.skyLightSent, buf);
        }

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