#pragma once
#include <string>
#include <cstdint>
#include "../../Packet.hpp"
#include "../../ClientBoundPacket.hpp"
#include "net/lily/minecpp/world/World.hpp"
#include "net/lily/minecpp/world/WorldType.hpp"

class S01PacketJoinGame : public ClientBoundPacket {
public:
    int32_t entityId = 0;
    bool hardcoreMode = false;
    GameType::Value gameType = GameType::SURVIVAL;
    int8_t dimension = 0;
    EnumDifficulty::Value difficulty = EnumDifficulty::PEACEFUL;
    uint8_t maxPlayers = 0;
    const WorldType *worldType = WorldType::DEFAULT;
    bool reducedDebugInfo = false;

    S01PacketJoinGame() : ClientBoundPacket(0x01) {}

    S01PacketJoinGame(const int32_t entityId, const GameType::Value gameType, const bool hardcoreMode, const int8_t dimension,
                      const EnumDifficulty::Value difficulty, const uint8_t maxPlayers, const WorldType* worldType, const bool reducedDebugInfo)
        : ClientBoundPacket(0x01), entityId(entityId), gameType(gameType), hardcoreMode(hardcoreMode),
          dimension(dimension), difficulty(difficulty), maxPlayers(maxPlayers), worldType(worldType),
          reducedDebugInfo(reducedDebugInfo) {}

    static S01PacketJoinGame deserialize(const std::vector<uint8_t>& data) {
        S01PacketJoinGame packet;
        size_t offset = 0;
        packet.entityId = readInt(data, offset);
        uint8_t i = readByte(data, offset);
        packet.hardcoreMode = (i & 8) != 0;
        i &= ~8;
        packet.gameType = GameType::fromID(i);
        packet.dimension = static_cast<int8_t>(readByte(data, offset));
        packet.difficulty = EnumDifficulty::fromId(readByte(data, offset));
        packet.maxPlayers = readByte(data, offset);
        const std::string typeName = readString(data, offset, 16);
        packet.worldType = WorldType::parseWorldType(typeName);
        if (!packet.worldType) {
            packet.worldType = WorldType::DEFAULT;
        }
        packet.reducedDebugInfo = readByte(data, offset) != 0;
        return packet;
    }
};
