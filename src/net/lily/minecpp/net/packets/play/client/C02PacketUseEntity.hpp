#pragma once

#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"

class C02PacketUseEntity final : public ServerBoundPacket {
public:
    enum class Action : uint32_t { Interact = 0, Attack = 1, InteractAt = 2 };

    C02PacketUseEntity(const int entityId, const Action action, const glm::vec3& hitOffset = {})
        : ServerBoundPacket(0x02), entityId(entityId), action(action), hitOffset(hitOffset) {}

    [[nodiscard]] PacketBuffer serialize() const override {
        buf.writeVarInt(static_cast<uint32_t>(entityId));
        buf.writeVarInt(static_cast<uint32_t>(action));
        if (action == Action::InteractAt) {
            buf.writeFloat(hitOffset.x);
            buf.writeFloat(hitOffset.y);
            buf.writeFloat(hitOffset.z);
        }
        return buf;
    }

private:
    int entityId;
    Action action;
    glm::vec3 hitOffset;
};
