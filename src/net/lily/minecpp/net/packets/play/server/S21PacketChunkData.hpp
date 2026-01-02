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

    struct ChunkSection {
        std::vector<uint16_t> blocks;      // 16*16*16 blocks, 2 bytes each
        std::vector<uint8_t> blockLight;   // 2048 bytes (4-bit nibbles)
        std::vector<uint8_t> skyLight;     // 2048 bytes, only for overworld
    };

    struct Extracted {
        std::vector<ChunkSection> sections; // up to 16 sections
        std::vector<uint8_t> biomes;        // 256 bytes
        uint16_t primaryBitmask = 0;
    } extractedData;

    S21PacketChunkData() : ClientBoundPacket(0x21) {}

    static S21PacketChunkData deserialize(const std::vector<uint8_t>& buffer) {
        S21PacketChunkData packet;
        size_t offset = 0;

        if (buffer.size() < 4 + 4 + 1 + 2)
            throw std::runtime_error("Buffer too small for S21PacketChunkData header");

        packet.chunkX = (buffer[offset] << 24) | (buffer[offset + 1] << 16) |
                        (buffer[offset + 2] << 8) | buffer[offset + 3];
        offset += 4;

        packet.chunkZ = (buffer[offset] << 24) | (buffer[offset + 1] << 16) |
                        (buffer[offset + 2] << 8) | buffer[offset + 3];
        offset += 4;

        packet.groundUp = buffer[offset++] != 0;

        if (offset + 2 > buffer.size()) throw std::runtime_error("Buffer too small for primary bitmask");
        packet.extractedData.primaryBitmask = (buffer[offset] << 8) | buffer[offset + 1];
        offset += 2;

        const size_t dataLength = getDataLength(
            __builtin_popcount(packet.extractedData.primaryBitmask),
            packet.groundUp, true
        );
        if (offset + dataLength > buffer.size()) throw std::runtime_error("Buffer too small for chunk data");

        packet.extractedData.sections.resize(16);
        size_t dataOffset = offset;
        for (int sectionY = 0; sectionY < 16; ++sectionY) {
            if ((packet.extractedData.primaryBitmask & (1 << sectionY)) == 0) continue;

            ChunkSection section;

            // 16*16*16 blocks, 2 bytes each
            section.blocks.resize(16*16*16);
            for (size_t b = 0; b < section.blocks.size(); ++b) {
                section.blocks[b] = static_cast<uint16_t>(buffer[dataOffset]) |
                                    (static_cast<uint16_t>(buffer[dataOffset + 1]) << 8);
                dataOffset += 2;
            }

            // block light
            section.blockLight.insert(section.blockLight.end(),
                                      buffer.begin() + dataOffset,
                                      buffer.begin() + dataOffset + 2048);
            dataOffset += 2048;

            // sky light (if overworld)
            if (packet.groundUp) {
                section.skyLight.insert(section.skyLight.end(),
                                        buffer.begin() + dataOffset,
                                        buffer.begin() + dataOffset + 2048);
                dataOffset += 2048;
            }

            packet.extractedData.sections[sectionY] = std::move(section);
        }

        // biome array
        packet.extractedData.biomes.insert(packet.extractedData.biomes.end(),
                                           buffer.begin() + dataOffset,
                                           buffer.begin() + dataOffset + 256);

        return packet;
    }

    static int getDataLength(int sectionCount, bool isOverworld, bool hasBiome) {
        int blocks = sectionCount * 2 * 16*16*16;           // 2 bytes per block
        int blockLight = sectionCount * 16*16*16 / 2;       // 4-bit nibbles
        int skyLight = isOverworld ? sectionCount * 16*16*16 / 2 : 0;
        int biome = hasBiome ? 256 : 0;
        return blocks + blockLight + skyLight + biome;
    }
};
