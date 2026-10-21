#pragma once

#include <cstdint>
#include <string>

#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"
// string limits are in characters in the protocol, up to four bytes each (a section sign is two), so the byte limit is 4x

struct S3DPacketDisplayScoreboard {
    uint8_t position = 0;
    std::string objective;

    static S3DPacketDisplayScoreboard deserialize(const PacketBuffer& buffer) {
        return {buffer.readByte(), buffer.readString(64)};
    }
};
