#pragma once
#include <vector>
#include <string>
#include <stdexcept>
#include "../../Packet.hpp"
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"
#include "net/lily/minecpp/world/World.hpp"
#include "net/lily/minecpp/world/WorldType.hpp"

class S07PacketRespawn : public ClientBoundPacket {
public:
    int32_t dimensionID = 0;
    EnumDifficulty::Value difficulty = EnumDifficulty::PEACEFUL;
    GameType::Value gameType = GameType::SURVIVAL;
    WorldType* worldType = WorldType::DEFAULT;

    S07PacketRespawn() : ClientBoundPacket(0x07) {}

    S07PacketRespawn(int32_t dim, EnumDifficulty::Value diff, WorldType* type, GameType::Value game)
        : ClientBoundPacket(0x07), dimensionID(dim), difficulty(diff), gameType(game), worldType(type) {}

    static S07PacketRespawn deserialize(const std::vector<uint8_t>& data) {
        S07PacketRespawn packet;
        size_t offset = 0;

        if (offset + 4 > data.size()) throw std::runtime_error("Buffer too small for dimensionID");
        packet.dimensionID = data[offset] << 24 | data[offset + 1] << 16 |
                             data[offset + 2] << 8 | data[offset + 3];
        offset += 4;

        if (offset >= data.size()) throw std::runtime_error("Buffer too small for difficulty");
        packet.difficulty = EnumDifficulty::fromId(data[offset++]);

        if (offset >= data.size()) throw std::runtime_error("Buffer too small for gameType");
        packet.gameType = GameType::fromID(data[offset++]);

        packet.worldType = WorldType::parseWorldType(readString(data, offset, 16));
        if (!packet.worldType) {
            packet.worldType = WorldType::DEFAULT;
        }

        return packet;
    }
};
