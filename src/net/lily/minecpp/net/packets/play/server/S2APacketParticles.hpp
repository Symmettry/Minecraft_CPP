#pragma once
#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"

struct S2APacketParticles {
    int32_t type{};
    bool longDistance{};
    glm::vec3 position{};
    glm::vec3 offset{};
    float speed{};
    int32_t count{};
    int32_t arguments[2]{};

    static S2APacketParticles deserialize(const PacketBuffer& buffer) {
        S2APacketParticles packet;
        packet.type = buffer.readInt();
        packet.longDistance = buffer.readBool();
        packet.position = {buffer.readFloat(), buffer.readFloat(), buffer.readFloat()};
        packet.offset = {buffer.readFloat(), buffer.readFloat(), buffer.readFloat()};
        packet.speed = buffer.readFloat();
        packet.count = buffer.readInt();
        // item crack (36) carries an item id and damage, block crack (37) and block dust (38) one packed block
        const int argumentCount = packet.type == 36 ? 2 : (packet.type == 37 || packet.type == 38) ? 1 : 0;
        for (int index = 0; index < argumentCount; ++index)
            packet.arguments[index] = static_cast<int32_t>(buffer.readVarInt());
        return packet;
    }
};
