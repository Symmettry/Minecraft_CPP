#pragma once

#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"

struct S09PacketHeldItemChange {
    int8_t slot{};

    static S09PacketHeldItemChange deserialize(const PacketBuffer& buffer) {
        return {static_cast<int8_t>(buffer.readByte())};
    }
};
