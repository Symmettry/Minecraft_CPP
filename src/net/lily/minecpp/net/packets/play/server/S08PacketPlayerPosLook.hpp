#pragma once
#include <vector>
#include <set>
#include <stdexcept>
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

class S08PacketPlayerPosLook final : public ClientBoundPacket {
public:
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float yaw = 0.0f;
    float pitch = 0.0f;

    enum class EnumFlags : uint8_t {
        X = 0,
        Y = 1,
        Z = 2,
        Y_ROT = 3,
        X_ROT = 4
    };

    std::set<EnumFlags> flags;

    S08PacketPlayerPosLook() : ClientBoundPacket(0x08) {}

    static S08PacketPlayerPosLook deserialize(const PacketBuffer& buf) {
        S08PacketPlayerPosLook packet;

        packet.x = buf.readDouble();
        packet.y = buf.readDouble();
        packet.z = buf.readDouble();
        packet.yaw = buf.readFloat();
        packet.pitch = buf.readFloat();

        const uint8_t mask = buf.readByte();
        for (int i = 0; i <= 4; ++i) {
            if (mask & (1 << i)) {
                packet.flags.insert(static_cast<EnumFlags>(i));
            }
        }

        return packet;
    }
};
