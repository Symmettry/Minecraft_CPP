#pragma once

#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"

class C0BPacketEntityAction final : public ServerBoundPacket {
public:
    enum class Action : uint32_t {
        StartSneaking = 0,
        StopSneaking = 1,
        LeaveBed = 2,
        StartSprinting = 3,
        StopSprinting = 4,
        RidingJump = 5,
        OpenInventory = 6,
    };

    C0BPacketEntityAction(const int entityId, const Action action, const int auxiliaryData = 0)
        : ServerBoundPacket(0x0B), entityId(entityId), action(action), auxiliaryData(auxiliaryData) {}

    [[nodiscard]] PacketBuffer serialize() const override {
        buf.writeVarInt(static_cast<uint32_t>(entityId));
        buf.writeVarInt(static_cast<uint32_t>(action));
        buf.writeVarInt(static_cast<uint32_t>(auxiliaryData));
        return buf;
    }

private:
    int entityId;
    Action action;
    int auxiliaryData;
};
