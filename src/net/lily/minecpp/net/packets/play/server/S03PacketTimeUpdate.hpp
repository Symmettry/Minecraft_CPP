#pragma once

#include <vector>
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

#define INT64 static_cast<int64_t>

class S03PacketTimeUpdate final : public ClientBoundPacket {
public:
    int64_t totalWorldTime = 0;
    int64_t worldTime = 0;

    S03PacketTimeUpdate() : ClientBoundPacket(0x03) {}

    S03PacketTimeUpdate(const int64_t totalWorldTimeIn, const int64_t worldTimeIn, const bool doDayLightCycle)
        : ClientBoundPacket(0x03), totalWorldTime(totalWorldTimeIn), worldTime(worldTimeIn) {
        if (!doDayLightCycle) {
            worldTime = -worldTime;
            if (worldTime == 0) worldTime = -1;
        }
    }

    static S03PacketTimeUpdate deserialize(const PacketBuffer& buf) {
        S03PacketTimeUpdate packet;

        packet.totalWorldTime = buf.readULong();
        packet.worldTime = buf.readULong();

        return packet;
    }
};
