#pragma once
#include <vector>
#include <cstdint>
#include "../../Packet.hpp"
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

class S1FPacketSetExperience : public ClientBoundPacket {
public:
    float experience = 0.0f;
    uint64_t experienceTotal = 0, experienceLevel = 0;

    S1FPacketSetExperience() : ClientBoundPacket(0x1F) {}

    S1FPacketSetExperience(float expBar, int totalExp, int lvl)
        : ClientBoundPacket(0x1F), experience(expBar), experienceTotal(totalExp), experienceLevel(lvl) {}

    static S1FPacketSetExperience deserialize(const std::vector<uint8_t>& data) {
        S1FPacketSetExperience packet;
        size_t offset = 0;

        packet.experience = readFloat(data, offset);
        packet.experienceLevel = readVarInt(data, offset);
        packet.experienceTotal = readVarInt(data, offset);

        return packet;
    }
};
