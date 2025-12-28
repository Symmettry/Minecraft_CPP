#pragma once
#include <vector>
#include <cstdint>
#include "../../Packet.hpp"
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

#define INT64 static_cast<int64_t>

class S03PacketTimeUpdate : public ClientBoundPacket {
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

    static S03PacketTimeUpdate deserialize(const std::vector<uint8_t>& data) {
        S03PacketTimeUpdate packet;
        size_t offset = 0;
        if (offset + 16 > data.size()) throw std::runtime_error("Buffer too small for S03PacketTimeUpdate");

        packet.totalWorldTime = INT64(data[offset]) << 56 |
                                INT64(data[offset + 1]) << 48 |
                                INT64(data[offset + 2]) << 40 |
                                INT64(data[offset + 3]) << 32 |
                                INT64(data[offset + 4]) << 24 |
                                INT64(data[offset + 5]) << 16 |
                                INT64(data[offset + 6]) << 8 |
                                INT64(data[offset + 7]);
        offset += 8;

        packet.worldTime = INT64(data[offset]) << 56 |
                           INT64(data[offset + 1]) << 48 |
                           INT64(data[offset + 2]) << 40 |
                           INT64(data[offset + 3]) << 32 |
                           INT64(data[offset + 4]) << 24 |
                           INT64(data[offset + 5]) << 16 |
                           INT64(data[offset + 6]) << 8 |
                           INT64(data[offset + 7]);

        return packet;
    }
};
