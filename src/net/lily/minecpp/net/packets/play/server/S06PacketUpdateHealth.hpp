#pragma once
#include <vector>
#include <cstdint>
#include <stdexcept>
#include "../../Packet.hpp"
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

class S06PacketUpdateHealth : public ClientBoundPacket {
public:
    float health = 0.0f;
    int32_t foodLevel = 0;
    float saturationLevel = 0.0f;

    S06PacketUpdateHealth() : ClientBoundPacket(0x06) {}

    S06PacketUpdateHealth(const float healthIn, const int32_t foodLevelIn, const float saturationIn)
        : ClientBoundPacket(0x06), health(healthIn), foodLevel(foodLevelIn), saturationLevel(saturationIn) {}

    static S06PacketUpdateHealth deserialize(const std::vector<uint8_t>& data) {
        S06PacketUpdateHealth packet;
        size_t offset = 0;

        if (offset + 4 > data.size()) throw std::runtime_error("Buffer too small for health");
        packet.health = *reinterpret_cast<const float*>(&data[offset]);
        offset += 4;

        packet.foodLevel = static_cast<int32_t>(packet.readVarInt(data, offset));

        if (offset + 4 > data.size()) throw std::runtime_error("Buffer too small for saturationLevel");
        packet.saturationLevel = *reinterpret_cast<const float*>(&data[offset]);
        offset += 4;

        return packet;
    }
};
