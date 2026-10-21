#pragma once
#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"

struct S2BPacketChangeGameState {
    uint8_t reason{};
    float value{};

    static S2BPacketChangeGameState deserialize(const PacketBuffer& buffer) {
        S2BPacketChangeGameState packet;
        packet.reason = buffer.readByte();
        packet.value = buffer.readFloat();
        return packet;
    }
};
