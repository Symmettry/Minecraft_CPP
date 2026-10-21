#pragma once

#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"

class C09PacketHeldItemChange final : public ServerBoundPacket {
public:
    explicit C09PacketHeldItemChange(const int slot)
        : ServerBoundPacket(0x09), slotId(slot) {}

    [[nodiscard]] PacketBuffer serialize() const override {
        buf.writeUShort(static_cast<uint16_t>(slotId));
        return buf;
    }

private:
    int slotId;
};
