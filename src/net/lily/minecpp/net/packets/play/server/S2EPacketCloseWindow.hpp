#pragma once

#include <cstdint>

#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"

struct S2EPacketCloseWindow {
    uint8_t windowId{};

    static S2EPacketCloseWindow deserialize(const PacketBuffer& buffer) {
        return {buffer.readByte()};
    }
};
