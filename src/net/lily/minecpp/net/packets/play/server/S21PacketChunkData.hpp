#pragma once
#include <vector>
#include <cstdint>
#include <stdexcept>
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

class S21PacketChunkData final : public ClientBoundPacket {
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

    static S21PacketChunkData deserialize(const PacketBuffer& buf, const bool isOverworld) {
        S21PacketChunkData packet;

        packet.chunkX = buf.readInt();
        packet.chunkZ = buf.readInt();

        packet.groundUp = buf.readBool();
        packet.chunk.bitMask = buf.readShort();
        packet.chunk.size = buf.readVarInt();

        deserializeChunk(packet.chunk, packet.groundUp, isOverworld, buf);

        buf.confirm();

        return packet;
    }

    static void deserializeChunk(Chunk& chunk, const bool continuous, const bool skyLightSent, const PacketBuffer& buf) {
        for (int s = 0; s < 16; s++) {
            auto&[data, blockLight, skyLight, isSkipped] = chunk.sections[s];
            isSkipped = !(chunk.bitMask & 1 << s);
            if (isSkipped) continue;

            for (int b = 0; b < 4096; b++)
                data[b] = buf.readBlock();

            for (int l = 0; l < 2048; l++) {
                const uint8_t light = buf.readByte();
                blockLight[l * 2] = light >> 4;
                blockLight[l * 2 + 1] = light & 0x0F;
            }

            if (skyLightSent) {
                for (int l = 0; l < 2048; l++) {
                    const uint8_t light = buf.readByte();
                    skyLight[l * 2] = light >> 4;
                    skyLight[l * 2 + 1] = light & 0x0F;
                }
            }
        }

        if (continuous) {
            for (int c = 0; c < 256; c++)
                chunk.biomes[c] = buf.readByte();
        }
    }

};