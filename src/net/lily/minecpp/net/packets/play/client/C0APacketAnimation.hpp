#pragma once

#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"

class C0APacketAnimation final : public ServerBoundPacket {
public:
    C0APacketAnimation() : ServerBoundPacket(0x0A) {}
    [[nodiscard]] PacketBuffer serialize() const override { return buf; }
};
