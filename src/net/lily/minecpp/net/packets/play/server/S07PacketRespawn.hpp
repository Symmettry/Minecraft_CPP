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

    static S07PacketRespawn deserialize(const PacketBuffer& buf) {
        S07PacketRespawn packet;

        packet.dimensionID = buf.readInt();
        packet.difficulty = EnumDifficulty::fromId(buf.readByte());
        packet.gameType = GameType::fromID(buf.readByte());
        packet.worldType = WorldType::parseWorldType(buf.readString(16));
        if (!packet.worldType) {
            packet.worldType = WorldType::DEFAULT;
        }

        return packet;
    }
};
