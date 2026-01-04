#pragma once
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

class S06PacketUpdateHealth final : public ClientBoundPacket {
public:
    float health = 0.0f;
    int32_t foodLevel = 0;
    float saturationLevel = 0.0f;

    S06PacketUpdateHealth() : ClientBoundPacket(0x06) {}

    static S06PacketUpdateHealth deserialize(const PacketBuffer& buf) {
        S06PacketUpdateHealth packet;

        packet.health = buf.readFloat();
        packet.foodLevel = buf.readVarInt();
        packet.saturationLevel = buf.readFloat();

        return packet;
    }
};
