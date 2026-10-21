#pragma once

#include <cstdint>
#include <string>

#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"
// string limits are in characters in the protocol, up to four bytes each (a section sign is two), so the byte limit is 4x

struct S3BPacketScoreboardObjective {
    std::string name;
    std::string displayName;
    std::string renderType = "integer";
    uint8_t action = 0;

    static S3BPacketScoreboardObjective deserialize(const PacketBuffer& buffer) {
        S3BPacketScoreboardObjective packet;
        packet.name = buffer.readString(64);
        packet.action = buffer.readByte();
        switch (packet.action) {
            case 0: case 2:
                packet.displayName = buffer.readString(128);
                packet.renderType = buffer.readString(64);
                break;
            default: break;
        }
        return packet;
    }
};
