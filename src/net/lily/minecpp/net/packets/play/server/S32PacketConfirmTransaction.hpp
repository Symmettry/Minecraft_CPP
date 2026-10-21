#pragma once

#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"

struct S32PacketConfirmTransaction {
    uint8_t windowId{};
    int16_t actionNumber{};
    bool accepted{};

    static S32PacketConfirmTransaction deserialize(const PacketBuffer& buffer) {
        return {buffer.readByte(), static_cast<int16_t>(buffer.readUShort()), buffer.readBool()};
    }
};
