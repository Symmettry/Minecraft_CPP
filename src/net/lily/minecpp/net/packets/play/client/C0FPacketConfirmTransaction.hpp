#pragma once

#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"

class C0FPacketConfirmTransaction final : public ServerBoundPacket {
public:
    C0FPacketConfirmTransaction(const uint8_t windowId, const int16_t actionNumber,
                                const bool accepted)
        : ServerBoundPacket(0x0F), windowId(windowId), actionNumber(actionNumber),
          accepted(accepted) {}

    [[nodiscard]] PacketBuffer serialize() const override {
        buf.writeByte(windowId);
        buf.writeUShort(static_cast<uint16_t>(actionNumber));
        buf.writeBool(accepted);
        return buf;
    }

private:
    uint8_t windowId;
    int16_t actionNumber;
    bool accepted;
};
