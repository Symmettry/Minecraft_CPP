#pragma once

#include <algorithm>
#include <glm/glm.hpp>

#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"
#include "net/lily/minecpp/net/packets/play/server/EntityMetadata.hpp"

class C08PacketPlayerBlockPlacement final : public ServerBoundPacket {
public:
    C08PacketPlayerBlockPlacement(const glm::ivec3 position, const uint8_t facing,
                                  const NetworkItemStack& stack, const glm::vec3 hit)
        : ServerBoundPacket(0x08), position(position), facing(facing), stack(stack), hit(hit) {}

    [[nodiscard]] PacketBuffer serialize() const override {
        buf.writePosition(position);
        buf.writeByte(facing);
        EntityMetadataDetail::writeItemStack(buf, stack);
        buf.writeByte(static_cast<int>(std::clamp(hit.x, 0.0f, 1.0f) * 16.0f));
        buf.writeByte(static_cast<int>(std::clamp(hit.y, 0.0f, 1.0f) * 16.0f));
        buf.writeByte(static_cast<int>(std::clamp(hit.z, 0.0f, 1.0f) * 16.0f));
        return buf;
    }

private:
    glm::ivec3 position;
    uint8_t facing;
    NetworkItemStack stack;
    glm::vec3 hit;
};
