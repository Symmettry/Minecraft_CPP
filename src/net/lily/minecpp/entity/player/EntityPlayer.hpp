#pragma once
#include "net/lily/minecpp/net/packets/play/server/S01PacketJoinGame.hpp"
#include "net/lily/minecpp/net/packets/play/server/S1FPacketSetExperience.hpp"
#include "net/lily/minecpp/world/World.hpp"
#ifndef PLAYER_HPP

#include "../Entity.hpp"

class EntityPlayer : public Entity {
public:

    mutable float experience = 0.0f;
    mutable uint64_t experienceTotal = 0, experienceLevel = 0;
    bool reducedDebug = false;
    int dimension = 0;
    GameType::Value gameType = GameType::SURVIVAL;

    explicit EntityPlayer(const Minecraft* mc, const float x, const float y, const float z) : Entity(mc, x, y, z) {
        width = 0.6f;
    }

    double getEyeHeight() const override {
        return isSneaking() ? 1.54f : 1.62f;
    }

    void update() override;

    void moveEntityWithHeading(float strafe, float forward) override;

    void handleMouseLook();
    void handleSprintToggle();
    void handleKeyboardInput();

    float getAIMoveSpeed() const override {
        return 0.1f; // move speed attribute; todo
    }

    void setXPStats(const float xp, const uint64_t xpTotal, const uint64_t xpLevel) const {
        experience = xp;
        experienceTotal = xpTotal;
        experienceLevel = xpLevel;
    }

    mutable glm::vec<3, double> lastReportedPos{0,0,0};
    mutable Rotation lastReportedRot{0,0};
    int positionUpdateTicks = 0;

    int foodLevel = 20;
    float foodSaturationLevel = 5.0f;
    bool serverSneakState = false, serverSprintState = false;
    bool suppressPhysics = false;

    std::string username = "lily6";
};

struct EnumPlayerModelParts {
    enum Value {
        CAPE,
        JACKET,
        LEFT_SLEEVE,
        RIGHT_SLEEVE,
        LEFT_PANTS_LEG,
        RIGHT_PANTS_LEG,
        HAT,
    } value;

    static int getMask(const int value) {
        switch (value) {
            case CAPE: return 1 << 0;
            case JACKET: return 1 << 1;
            case LEFT_SLEEVE: return 1 << 2;
            case RIGHT_SLEEVE: return 1 << 3;
            case LEFT_PANTS_LEG: return 1 << 4;
            case RIGHT_PANTS_LEG: return 1 << 5;
            case HAT: return 1 << 6;
            default: throw std::runtime_error("Not possible.");
        }
    }
    static int getMask(const Value value) {
        return getMask(static_cast<int>(value));
    }
};

#endif