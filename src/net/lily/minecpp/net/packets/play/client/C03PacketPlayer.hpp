#ifndef MINECRAFTCLIENT_C03PACKETPLAYER_HPP
#define MINECRAFTCLIENT_C03PACKETPLAYER_HPP
#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"

class C03PacketPlayer : public ServerBoundPacket {
public:
    double x = 0.0, y = 0.0, z = 0.0;
    float yaw = 0.0f, pitch = 0.0f;
    bool onGround = false;

    explicit C03PacketPlayer(const bool isOnGround)
        : ServerBoundPacket(0x03), onGround(isOnGround) {}

    PacketBuffer serialize() const override {
        // C03 only sends onGround
        buf.writeBool(onGround);
        return buf;
    }
};

class C04PacketPlayerPosition final : public C03PacketPlayer {
public:
    C04PacketPlayerPosition(const double posX, const double posY, const double posZ, const bool isOnGround)
        : C03PacketPlayer(isOnGround) {
        id = 0x04;
        x = posX;
        y = posY;
        z = posZ;
    }

    PacketBuffer serialize() const override {
        buf.writeDouble(x);
        buf.writeDouble(y);
        buf.writeDouble(z);
        buf.writeBool(onGround);
        return buf;
    }
};

class C05PacketPlayerLook final : public C03PacketPlayer {
public:
    C05PacketPlayerLook(const float playerYaw, const float playerPitch, const bool isOnGround)
        : C03PacketPlayer(isOnGround) {
        id = 0x05;
        yaw = playerYaw;
        pitch = playerPitch;
    }

    PacketBuffer serialize() const override {
        buf.writeFloat(yaw);
        buf.writeFloat(pitch);
        buf.writeBool(onGround);
        return buf;
    }
};

class C06PacketPlayerPosLook final : public C03PacketPlayer {
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

    PacketBuffer serialize() const override {
        buf.writeDouble(x);
        buf.writeDouble(y);
        buf.writeDouble(z);
        buf.writeFloat(yaw);
        buf.writeFloat(pitch);
        buf.writeBool(onGround);
        return buf;
    }
};

#endif // MINECRAFTCLIENT_C03PACKETPLAYER_HPP