#pragma once

#include <cstdint>
#include <string>

#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"
// string limits are in characters in the protocol, up to four bytes each (a section sign is two), so the byte limit is 4x

struct S3CPacketUpdateScore {
    std::string player;
    std::string objective;
    uint32_t action = 0;
    int32_t value = 0;

    static S3CPacketUpdateScore deserialize(const PacketBuffer& buffer) {
        S3CPacketUpdateScore packet;
        packet.player = buffer.readString(160);
        packet.action = buffer.readVarInt();
        packet.objective = buffer.readString(64);
        if (packet.action != 1) packet.value = static_cast<int32_t>(buffer.readVarInt());
        return packet;
    }
};
