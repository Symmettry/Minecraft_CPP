#pragma once
#include <vector>
#include "../../Packet.hpp"
#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"

class C00PacketKeepAlive : public ServerBoundPacket {
public:
    int key = 0;

    C00PacketKeepAlive() : ServerBoundPacket(0x00) {}
    explicit C00PacketKeepAlive(const int key) : ServerBoundPacket(0x00), key(key) {}

    [[nodiscard]] std::vector<uint8_t> serialize() const override {
        std::vector<uint8_t> buffer;
        writeVarInt(static_cast<uint32_t>(key), buffer);
        return buffer;
    }
};
