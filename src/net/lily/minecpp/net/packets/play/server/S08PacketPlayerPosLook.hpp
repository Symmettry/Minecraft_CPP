#pragma once
#include <vector>
#include <cstdint>
#include <set>
#include <stdexcept>
#include "../../Packet.hpp"
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

class S08PacketPlayerPosLook : public ClientBoundPacket {
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

    S08PacketPlayerPosLook(const double xIn, const double yIn, const double zIn, const float yawIn, const float pitchIn, const std::set<EnumFlags>& flagsIn)
        : ClientBoundPacket(0x08), x(xIn), y(yIn), z(zIn), yaw(yawIn), pitch(pitchIn), flags(flagsIn) {}

    static S08PacketPlayerPosLook deserialize(const std::vector<uint8_t>& data) {
        S08PacketPlayerPosLook packet;
        size_t offset = 0;

        if (offset + 8*3 + 4*2 + 1 > data.size()) throw std::runtime_error("Buffer too small for S08PacketPlayerPosLook");

        packet.x = readDouble(data, offset);
        packet.y = readDouble(data, offset);
        packet.z = readDouble(data, offset);
        packet.yaw = readFloat(data, offset);
        packet.pitch = readFloat(data, offset);

        const uint8_t mask = data[offset++];
        for (int i = 0; i <= 4; ++i) {
            if (mask & (1 << i)) {
                packet.flags.insert(static_cast<EnumFlags>(i));
            }
        }

        return packet;
    }
};
