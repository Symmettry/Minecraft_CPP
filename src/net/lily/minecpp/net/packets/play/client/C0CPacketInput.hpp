#pragma once

#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"

// sent every tick while riding: steering input and the sneak flag that dismounts
class C0CPacketInput final : public ServerBoundPacket {
public:
    C0CPacketInput(const float strafe, const float forward, const bool jumping, const bool sneaking)
        : ServerBoundPacket(0x0C), strafe(strafe), forward(forward), jumping(jumping), sneaking(sneaking) {}

    [[nodiscard]] PacketBuffer serialize() const override {
        buf.writeFloat(strafe);
        buf.writeFloat(forward);
        buf.writeByte((jumping ? 1 : 0) | (sneaking ? 2 : 0));
        return buf;
    }

private:
    float strafe, forward;
    bool jumping, sneaking;
};
