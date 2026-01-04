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
        uint16_t data[4096];
        uint8_t blockLight[4096], skyLight[4096];
        bool isSkipped;
    } extractedData;

    struct Chunk {
        Extracted sections[16];
        uint8_t biomes[256];
        uint16_t bitMask;
        int size;
    } chunk;

    S21PacketChunkData() : ClientBoundPacket(0x21), extractedData(), chunk() {
    }

    static S21PacketChunkData deserialize(const std::vector<uint8_t>& buffer, const bool isOverworld) {
        S21PacketChunkData packet;
        size_t offset = 0;

        packet.chunkX = readInt(buffer, offset);
        packet.chunkZ = readInt(buffer, offset);

        packet.groundUp = readBool(buffer, offset);
        packet.chunk.bitMask = readShort(buffer, offset);
        packet.chunk.size = readVarInt(buffer, offset);

        deserializeChunk(packet.chunk, packet.groundUp, isOverworld, buffer, offset);

        confirm(buffer, offset);

        return packet;
    }

    static void deserializeChunk(Chunk& chunk, const bool continuous, const bool isOverworld, const std::vector<uint8_t>& buffer, size_t& offset) {
        for (int s = 0; s < 16; s++) {
            auto&[data, blockLight, skyLight, isSkipped] = chunk.sections[s];
            isSkipped = !(chunk.bitMask & 1 << s);
            if (isSkipped) continue;

            for (int b = 0; b < 4096; b++)
                data[b] = readBlock(buffer, offset);

            for (int l = 0; l < 2048; l++) {
                const uint8_t light = readByte(buffer, offset);
                blockLight[l * 2] = light >> 4;
                blockLight[l * 2 + 1] = light & 0x0F;
            }

            if (isOverworld) {
                for (int l = 0; l < 2048; l++) {
                    const uint8_t light = readByte(buffer, offset);
                    skyLight[l * 2] = light >> 4;
                    skyLight[l * 2 + 1] = light & 0x0F;
                }
            }
        }

        if (continuous) {
            for (int c = 0; c < 256; c++)
                chunk.biomes[c] = readByte(buffer, offset);
        }
    }

};