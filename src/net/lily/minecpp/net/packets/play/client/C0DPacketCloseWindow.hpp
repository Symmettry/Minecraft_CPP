#pragma once

#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"

class C0DPacketCloseWindow final : public ServerBoundPacket {
public:
    explicit C0DPacketCloseWindow(const uint8_t windowId)
        : ServerBoundPacket(0x0D), windowId(windowId) {}

    [[nodiscard]] PacketBuffer serialize() const override {
        buf.writeByte(windowId);
        return buf;
    }

private:
    uint8_t windowId;
};
