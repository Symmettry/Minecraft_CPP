#pragma once

#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"

class C13PacketPlayerAbilities final : public ServerBoundPacket {
public:
    C13PacketPlayerAbilities(const bool invulnerable, const bool flying, const bool allowFlying,
                             const bool creative, const float flySpeed, const float walkSpeed)
        : ServerBoundPacket(0x13), invulnerable(invulnerable), flying(flying),
          allowFlying(allowFlying), creative(creative), flySpeed(flySpeed), walkSpeed(walkSpeed) {}

    [[nodiscard]] PacketBuffer serialize() const override {
        int flags = 0;
        if (invulnerable) flags |= 1;
        if (flying) flags |= 2;
        if (allowFlying) flags |= 4;
        if (creative) flags |= 8;
        buf.writeByte(flags);
        buf.writeFloat(flySpeed);
        buf.writeFloat(walkSpeed);
        return buf;
    }

private:
    bool invulnerable, flying, allowFlying, creative;
    float flySpeed, walkSpeed;
};
