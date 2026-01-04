#pragma once
#include <string>
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
        : ClientBoundPacket(0x01), entityId(entityId), hardcoreMode(hardcoreMode), gameType(gameType),
          dimension(dimension), difficulty(difficulty), maxPlayers(maxPlayers), worldType(worldType),
          reducedDebugInfo(reducedDebugInfo) {}

    static S01PacketJoinGame deserialize(const PacketBuffer& buf) {
        S01PacketJoinGame packet;
        packet.entityId = buf.readInt();
        uint8_t i = buf.readByte();
        packet.hardcoreMode = (i & 8) != 0;
        i &= ~8;
        packet.gameType = GameType::fromID(i);
        packet.dimension = static_cast<int8_t>(buf.readByte());
        packet.difficulty = EnumDifficulty::fromId(buf.readByte());
        packet.maxPlayers = buf.readByte();
        const std::string typeName = buf.readString(16);
        packet.worldType = WorldType::parseWorldType(typeName);
        if (!packet.worldType) {
            packet.worldType = WorldType::DEFAULT;
        }
        packet.reducedDebugInfo = buf.readByte() != 0;
        return packet;
    }
};
