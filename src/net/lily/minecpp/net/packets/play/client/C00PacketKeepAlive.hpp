#pragma once

#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"

class C00PacketKeepAlive final : public ServerBoundPacket {
public:
    int key = 0;

    C00PacketKeepAlive() : ServerBoundPacket(0x00) {}
    explicit C00PacketKeepAlive(const int key) : ServerBoundPacket(0x00), key(key) {}

    [[nodiscard]] PacketBuffer serialize() const override {
        buf.writeVarInt(static_cast<uint32_t>(key));
        return buf;
    }
};
