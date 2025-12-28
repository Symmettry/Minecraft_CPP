#pragma once
#include <vector>
#include "../../Packet.hpp"
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"
#include "net/lily/minecpp/world/block/Block.hpp"

class S05PacketSpawnPosition : public ClientBoundPacket {
public:
    BlockPos spawnPos;

    S05PacketSpawnPosition() : ClientBoundPacket(0x05) {}

    explicit S05PacketSpawnPosition(const BlockPos& pos)
        : ClientBoundPacket(0x05), spawnPos(pos) {}

    static S05PacketSpawnPosition deserialize(const std::vector<uint8_t>& data) {
        S05PacketSpawnPosition packet;
        size_t offset = 0;
        // packet.spawnPos = BlockPos::deserialize(data, offset);
        return packet;
    }

};
