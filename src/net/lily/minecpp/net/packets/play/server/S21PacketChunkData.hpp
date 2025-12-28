#pragma once
#include <vector>
#include <cstdint>
#include <stdexcept>
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

class S21PacketChunkData : public ClientBoundPacket {
public:
    int chunkX = 0;
    int chunkZ = 0;
    bool groundUp = false;

    struct Extracted {
        std::vector<uint8_t> data;
        int dataSize = 0;
    } extractedData;

    S21PacketChunkData() : ClientBoundPacket(0x21) {}

    static S21PacketChunkData deserialize(const std::vector<uint8_t>& buffer) {
        S21PacketChunkData packet;
        size_t offset = 0;

        if (buffer.size() < 4 + 4 + 1 + 2) 
            throw std::runtime_error("Buffer too small for S21PacketChunkData header");

        // chunkX
        packet.chunkX = (buffer[offset] << 24) | (buffer[offset + 1] << 16) |
                        (buffer[offset + 2] << 8) | buffer[offset + 3];
        offset += 4;

        // chunkZ
        packet.chunkZ = (buffer[offset] << 24) | (buffer[offset + 1] << 16) |
                        (buffer[offset + 2] << 8) | buffer[offset + 3];
        offset += 4;

        // groundUp
        packet.groundUp = buffer[offset++] != 0;

        // data size
        if (offset + 2 > buffer.size()) throw std::runtime_error("Buffer too small for data size");
        packet.extractedData.dataSize = (buffer[offset] << 8) | buffer[offset + 1];
        offset += 2;

        // data
        if (offset + packet.extractedData.dataSize > buffer.size())
            throw std::runtime_error("Buffer too small for chunk data");
        packet.extractedData.data.insert(packet.extractedData.data.end(),
                                         buffer.begin() + offset,
                                         buffer.begin() + offset + packet.extractedData.dataSize);

        return packet;
    }

    static int func_180737_a(int p_180737_0_, bool p_180737_1_, bool p_180737_2_)
    {
        int i = p_180737_0_ * 2 * 16 * 16 * 16;
        int j = p_180737_0_ * 16 * 16 * 16 / 2;
        int k = p_180737_1_ ? p_180737_0_ * 16 * 16 * 16 / 2 : 0;
        int l = p_180737_2_ ? 256 : 0;
        return i + j + k + l;
    }

    const std::vector<uint8_t>& getData() const {
        return extractedData.data;
    }
};
