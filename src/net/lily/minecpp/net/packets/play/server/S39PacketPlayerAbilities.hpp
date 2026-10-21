#pragma once
#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"

struct S39PacketPlayerAbilities {
    uint8_t flags{};
    float flySpeed{};
    float walkSpeed{};

    static S39PacketPlayerAbilities deserialize(const PacketBuffer& buffer) {
        S39PacketPlayerAbilities packet;
        packet.flags = buffer.readByte();
        packet.flySpeed = buffer.readFloat();
        packet.walkSpeed = buffer.readFloat();
        return packet;
    }
};
