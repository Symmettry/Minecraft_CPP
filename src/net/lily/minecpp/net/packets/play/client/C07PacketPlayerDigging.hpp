#pragma once

#include <glm/glm.hpp>

#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"

class C07PacketPlayerDigging final : public ServerBoundPacket {
public:
    enum class Action : uint8_t {
        StartDestroyBlock = 0,
        AbortDestroyBlock = 1,
        StopDestroyBlock = 2,
        DropAllItems = 3,
        DropItem = 4,
        ReleaseUseItem = 5,
    };

    C07PacketPlayerDigging(const Action action, const glm::ivec3 position, const uint8_t facing)
        : ServerBoundPacket(0x07), action(action), position(position), facing(facing) {}

    [[nodiscard]] PacketBuffer serialize() const override {
        buf.writeVarInt(static_cast<uint32_t>(action));
        buf.writePosition(position);
        buf.writeByte(facing);
        return buf;
    }

private:
    Action action;
    glm::ivec3 position;
    uint8_t facing;
};
