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

        std::vector<uint16_t> primaryBitmasks(count);

        for (uint32_t i = 0; i < count; ++i) {
            packet.xPositions[i] = (buffer[offset] << 24) | (buffer[offset+1] << 16) |
                                   (buffer[offset+2] << 8) | buffer[offset+3];
            offset += 4;

            packet.zPositions[i] = (buffer[offset] << 24) | (buffer[offset+1] << 16) |
                                   (buffer[offset+2] << 8) | buffer[offset+3];
            offset += 4;

            primaryBitmasks[i] = (buffer[offset] << 8) | buffer[offset+1];
            offset += 2;

            offset += 2;

            packet.chunksData[i].sections.resize(16);
            packet.chunksData[i].biomes.resize(256);
        }

        for (uint32_t i = 0; i < count; ++i) {
            const uint16_t bitmask = primaryBitmasks[i];
            auto& extracted = packet.chunksData[i];

            for (int sectionY = 0; sectionY < 16; ++sectionY) {
                if ((bitmask & (1 << sectionY)) == 0) continue;

                S21PacketChunkData::ChunkSection section;

                // block data: 16*16*16 = 4096 blocks, 2 bytes each
                section.blocks.resize(16*16*16);
                for (size_t b = 0; b < section.blocks.size(); ++b) {
                    if (offset + 2 > buffer.size()) throw std::runtime_error("Unexpected end of chunk data");
                    section.blocks[b] = static_cast<uint16_t>(buffer[offset]) |
                                        (static_cast<uint16_t>(buffer[offset+1]) << 8);
                    offset += 2;
                }

                // block light: 2048 bytes (16*16*16 / 2 nibbles)
                if (offset + 2048 > buffer.size()) throw std::runtime_error("Unexpected end of block light data");
                section.blockLight.insert(section.blockLight.end(), buffer.begin()+offset, buffer.begin()+offset+2048);
                offset += 2048;

                // skylight: 2048 bytes (overworld only tho)
                if (packet.isOverworld) {
                    if (offset + 2048 > buffer.size()) throw std::runtime_error("Unexpected end of skylight data");
                    section.skyLight.insert(section.skyLight.end(), buffer.begin()+offset, buffer.begin()+offset+2048);
                    offset += 2048;
                }

                extracted.sections[sectionY] = std::move(section);
            }

            // biome array
            if (offset + 256 > buffer.size()) throw std::runtime_error("Unexpected end of biome data");
            std::ranges::copy_n(buffer.begin()+offset, 256, extracted.biomes.begin());
            offset += 256;
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
