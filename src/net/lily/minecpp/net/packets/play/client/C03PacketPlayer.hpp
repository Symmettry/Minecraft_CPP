#ifndef MINECRAFTCLIENT_C03PACKETPLAYER_HPP
#define MINECRAFTCLIENT_C03PACKETPLAYER_HPP
#include <cstdint>
#include <vector>
#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"

class C03PacketPlayer : public ServerBoundPacket {
public:
    double x = 0.0, y = 0.0, z = 0.0;
    float yaw = 0.0f, pitch = 0.0f;
    bool onGround = false;

    explicit C03PacketPlayer(bool isOnGround)
        : ServerBoundPacket(0x03), onGround(isOnGround) {}

    std::vector<uint8_t> serialize() const override {
        std::vector<uint8_t> buffer;
        // C03 only sends onGround
        writeBool(onGround, buffer);
        return buffer;
    }
};

class C04PacketPlayerPosition : public C03PacketPlayer {
public:
    C04PacketPlayerPosition(double posX, double posY, double posZ, bool isOnGround)
        : C03PacketPlayer(isOnGround) {
        id = 0x04;
        x = posX;
        y = posY;
        z = posZ;
    }

    std::vector<uint8_t> serialize() const override {
        std::vector<uint8_t> buffer;
        writeDouble(x, buffer);
        writeDouble(y, buffer);
        writeDouble(z, buffer);
        writeBool(onGround, buffer);
        return buffer;
    }
};

class C05PacketPlayerLook : public C03PacketPlayer {
public:
    C05PacketPlayerLook(float playerYaw, float playerPitch, bool isOnGround)
        : C03PacketPlayer(isOnGround) {
        id = 0x05;
        yaw = playerYaw;
        pitch = playerPitch;
    }

    std::vector<uint8_t> serialize() const override {
        std::vector<uint8_t> buffer;
        writeFloat(yaw, buffer);
        writeFloat(pitch, buffer);
        writeBool(onGround, buffer);
        return buffer;
    }
};

class C06PacketPlayerPosLook : public C03PacketPlayer {
public:
    C06PacketPlayerPosLook(const double playerX, const double playerY, const double playerZ,
                           const float playerYaw, const float playerPitch, const bool playerIsOnGround)
        : C03PacketPlayer(playerIsOnGround) {
        id = 0x06;
        x = playerX;
        y = playerY;
        z = playerZ;
        yaw = playerYaw;
        pitch = playerPitch;
    }

    std::vector<uint8_t> serialize() const override {
        std::vector<uint8_t> buffer;
        writeDouble(x, buffer);
        writeDouble(y, buffer);
        writeDouble(z, buffer);
        writeFloat(yaw, buffer);
        writeFloat(pitch, buffer);
        writeBool(onGround, buffer);
        return buffer;
    }
};

#endif // MINECRAFTCLIENT_C03PACKETPLAYER_HPP