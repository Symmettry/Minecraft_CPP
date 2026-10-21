#pragma once

#include <array>
#include "net/lily/minecpp/net/packets/play/server/S01PacketJoinGame.hpp"
#include "net/lily/minecpp/net/packets/play/server/S1FPacketSetExperience.hpp"
#include "net/lily/minecpp/world/World.hpp"
#ifndef PLAYER_HPP

#include "../Entity.hpp"
#include "net/lily/minecpp/net/packets/play/server/EntityMetadata.hpp"

class EntityPlayer : public Entity {
public:
    std::array<NetworkItemStack, 45> inventory{};
    NetworkItemStack carriedItem{};
    std::array<NetworkItemStack, 4> armor{}; // remote players only: boots, leggings, chestplate, helmet
    NetworkItemStack heldItem{}; // remote players only; the local player uses inventory
    int16_t inventoryTransactionId = 0;
    int currentHotbarSlot = 0;
    bool usingItem = false;
    int itemUseTicks = 0;
    int16_t itemInUseId = -1;
    int16_t itemInUseDamage = 0;

    mutable float experience = 0.0f;
    mutable uint64_t experienceTotal = 0, experienceLevel = 0;
    bool reducedDebug = false;
    int dimension = 0;
    GameType::Value gameType = GameType::SURVIVAL;
    double movementSpeed = 0.1;

    explicit EntityPlayer(const Minecraft* mc, const float x, const float y, const float z) : Entity(mc, x, y, z) {
        width = 0.6f;
    }

    double getEyeHeight() const override {
        return isSneaking() ? 1.54f : 1.62f;
    }

    float getAIMoveSpeed() const override {
        const double speed = isSprinting() ? movementSpeed * 1.3 : movementSpeed;
        return static_cast<float>(speed);
    }

    void setXPStats(const float xp, const uint64_t xpTotal, const uint64_t xpLevel) const {
        experience = xp;
        experienceTotal = xpTotal;
        experienceLevel = xpLevel;
    }

    EntityType getType() const override {
        return PLAYER;
    }

    bool isSpectator() const {
        return gameType == GameType::SPECTATOR;
    }

    // local player HUD state
    std::array<bool, 33> activeEffects{}; // potion ids: 10 regeneration, 17 hunger, 19 poison, 20 wither
    float absorption = 0.0f;
    int ridingEntityId = -1;
    int air = 300;
    bool hardcore = false;

    bool allowFlying = false, isFlying = false;
    float flySpeed = 0.05f;

    int foodLevel = 20;
    float foodSaturationLevel = 5.0f;
    float distanceWalkedModified = 0.0f;
    float prevDistanceWalkedModified = 0.0f;
    float cameraYaw = 0.0f;
    float prevCameraYaw = 0.0f;
    float cameraPitch = 0.0f;
    float prevCameraPitch = 0.0f;
    float fovModifier = 1.0f;
    float prevFovModifier = 1.0f;
    float renderYawOffset = 0.0f;
    float prevRenderYawOffset = 0.0f;
    float rotationYawHead = 0.0f;
    float prevRotationYawHead = 0.0f;
    float limbSwingDirection = 1.0f;
    float limbSwing = 0.0f;
    float limbSwingAmount = 0.0f;
    float prevLimbSwingAmount = 0.0f;
    float swingProgress = 0.0f;
    float prevSwingProgress = 0.0f;
    int swingProgressInt = 0;
    bool isSwingInProgress = false;

    void swingItem() {
        constexpr int animationTicks = 6;
        if (!isSwingInProgress || swingProgressInt >= animationTicks / 2 || swingProgressInt < 0) {
            swingProgressInt = -1;
            isSwingInProgress = true;
        }
    }

    void updateArmSwingProgress() {
        constexpr int animationTicks = 6;
        prevSwingProgress = swingProgress;
        if (isSwingInProgress && ++swingProgressInt >= animationTicks) {
            swingProgressInt = 0;
            isSwingInProgress = false;
        } else if (!isSwingInProgress) {
            swingProgressInt = 0;
        }
        swingProgress = static_cast<float>(swingProgressInt) / animationTicks;
    }

    std::string username = "lily7";
    std::string skinUrl;
    bool slimSkin = false;
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
