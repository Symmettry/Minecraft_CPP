#pragma once
#include <vector>
#include <cstdint>
#include <stdexcept>
#include "S21PacketChunkData.hpp"
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

class S26PacketMapChunkBulk : public ClientBoundPacket {
public:
    std::vector<int> xPositions;
    std::vector<int> zPositions;
    std::vector<S21PacketChunkData::Extracted> chunksData;
    bool isOverworld = true;

    S26PacketMapChunkBulk() : ClientBoundPacket(0x26) {}

    static S26PacketMapChunkBulk deserialize(const std::vector<uint8_t>& buffer) {
        S26PacketMapChunkBulk packet;
        size_t offset = 0;

        if (buffer.size() < 1) throw std::runtime_error("Buffer too small for S26 header");
        packet.isOverworld = buffer[offset++] != 0;

        const uint32_t count = readVarInt(buffer, offset);
        packet.xPositions.resize(count);
        packet.zPositions.resize(count);
        packet.chunksData.resize(count);

        for (uint32_t i = 0; i < count; ++i) {
            if (offset + 8 + 2 > buffer.size()) 
                throw std::runtime_error("Buffer too small for chunk headers");

            packet.xPositions[i] = (buffer[offset] << 24) | (buffer[offset + 1] << 16) |
                                   (buffer[offset + 2] << 8) | buffer[offset + 3];
            offset += 4;

            packet.zPositions[i] = (buffer[offset] << 24) | (buffer[offset + 1] << 16) |
                                   (buffer[offset + 2] << 8) | buffer[offset + 3];
            offset += 4;

            packet.chunksData[i].dataSize = (buffer[offset] << 8) | buffer[offset + 1];
            offset += 2;

            const size_t dataLength = S21PacketChunkData::func_180737_a(
                __builtin_popcount(packet.chunksData[i].dataSize),
                packet.isOverworld, true
            );
            packet.chunksData[i].data.resize(dataLength);
        }

        for (uint32_t i = 0; i < count; ++i) {
            if (offset + packet.chunksData[i].data.size() > buffer.size())
                throw std::runtime_error("Buffer too small for chunk data");
            std::copy_n(buffer.begin() + offset,
                      packet.chunksData[i].data.size(),
                      packet.chunksData[i].data.begin());
            offset += packet.chunksData[i].data.size();
        }

        return packet;
    }

    uint32_t getChunkCount() const {
        return xPositions.size();
    }

    const S21PacketChunkData::Extracted& getChunkData(const uint32_t index) const {
        return chunksData.at(index);
    }

    int getChunkX(const uint32_t index) const {
        return xPositions.at(index);
    }

    int getChunkZ(const uint32_t index) const {
        return zPositions.at(index);
    }
};
