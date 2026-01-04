#pragma once
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

class S1FPacketSetExperience final : public ClientBoundPacket {
public:
    float experience = 0.0f;
    uint64_t experienceTotal = 0, experienceLevel = 0;

    S1FPacketSetExperience() : ClientBoundPacket(0x1F) {}

    S1FPacketSetExperience(const float expBar, const int totalExp, const int lvl)
        : ClientBoundPacket(0x1F), experience(expBar), experienceTotal(totalExp), experienceLevel(lvl) {}

    static S1FPacketSetExperience deserialize(const PacketBuffer& buf) {
        S1FPacketSetExperience packet;

        packet.experience = buf.readFloat();
        packet.experienceLevel = buf.readVarInt();
        packet.experienceTotal = buf.readVarInt();

        return packet;
    }
};
