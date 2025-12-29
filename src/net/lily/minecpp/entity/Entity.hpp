#pragma once
#ifndef ENTITY_HPP
#define ENTITY_HPP

#include <memory>
#include <glm/glm.hpp>
#include <algorithm>
#include "net/lily/minecpp/util/AABB.hpp"
#include "net/lily/minecpp/util/Rotation.hpp"
#include "net/lily/minecpp/world/block/Block.hpp"

struct Minecraft;

class Entity : public std::enable_shared_from_this<Entity> {
public:
    const Minecraft* mc = nullptr;

    int entityId = 0;

    explicit Entity(const Minecraft* mc, double x, double y, double z);
    virtual ~Entity() = default;

    virtual void update();

    double getEyePos() const;
    double getLastEyePos() const;

    virtual void moveEntityWithHeading(float strafe, float forward);
    void moveFlying(float strafe, float forward, float friction) const;
    virtual void moveEntity(double dx, double dy, double dz);
    void resetPositionToBB() const;

    void jump();
    void onLivingUpdate();

    const Block *getBlockBelow() const;

    AABB getBoundingBox() const;

    void setBoundingBox(const AABB &box) const;

    void setPosition(double x, double y, double z) const;

    void setRotation(float yaw, float pitch) const;

    void setPositionAndRotation(double x, double y, double z, float yaw, float pitch) const;

    bool onGround = true, noClip = false, isInWeb = false, isAirBorne = false;
    float fallDistance = 0.0f, jumpMovementFactor = 0.02f, stepHeight = 0.6f;
    float width = 0.6f, height = 1.8f;

    mutable glm::vec<3, double> position{0,0,0};
    mutable glm::vec<3, double> lastPos{0,0,0};
    mutable glm::vec<3, double> velocity{0,0,0};

    mutable Rotation rotation{0.0, 0.0};
    mutable Rotation lastRot{0.0, 0.0};

    bool isCollidedHorizontally = false;
    bool isCollidedVertically = false;
    bool isCollided = false;

    int jumpTicks = 0;
    bool isJumping = false;

    float moveForward = 0.0f;
    float moveStrafe = 0.0f;
    bool sneaking = false;
    bool sprinting = false;

    float health = 10.0f;

    [[nodiscard]] virtual float getAIMoveSpeed() const { return 1.0f; }
    [[nodiscard]] virtual double getEyeHeight() const { return 1.62; }
    [[nodiscard]] virtual float jumpHeight() const { return 0.42f; }

    [[nodiscard]] virtual bool isOnLadder() const;
    [[nodiscard]] virtual bool isInLava() const { return false; }
    [[nodiscard]] virtual bool isInWater() const { return false; }

    [[nodiscard]] bool isCurrentViewEntity() const;

    void setSneaking(bool sneak) { sneaking = sneak; }
    bool isSneaking() const { return sneaking; }

    void setSprinting(bool sprint) { sprinting = sprint; }
    bool isSprinting() const { return sprinting; }

    std::string coordinates() const {
        return "X: " + std::to_string(position.x) + ", Y: " + std::to_string(position.y) + ", Z: " + std::to_string(position.z);
    }
    std::string rotations() const {
        return "Yaw: " + std::to_string(rotation.yaw) + ", Pitch: " + std::to_string(rotation.pitch);
    }

protected:
    mutable AABB boundingBox{};
};

#endif
