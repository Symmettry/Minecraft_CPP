#pragma once
#ifndef ENTITY_HPP
#define ENTITY_HPP

#include <memory>
#include <glm/glm.hpp>
#include <algorithm>
#include "net/lily/minecpp/util/AABB.hpp"
#include "net/lily/minecpp/util/Rotation.hpp"

struct Minecraft;

class Entity : public std::enable_shared_from_this<Entity> {
public:
    const Minecraft* mc = nullptr;

    explicit Entity(const Minecraft* mc, double x, double y, double z);
    virtual ~Entity() = default;

    // Update per tick
    virtual void update();

    // Eye position
    double getEyePos() const;
    double getLastEyePos() const;

    // Movement
    virtual void moveEntityWithHeading(double strafe, double forward);
    void moveFlying(double strafe, double forward, double friction);
    virtual void moveEntity(double dx, double dy, double dz);
    void resetPositionToBB() const;

    // Jump / living update
    void jump();
    void onLivingUpdate();

    // Bounding box
    AABB getBoundingBox() const;
    void setPosition(double x, double y, double z) const;

    // State
    bool onGround = true, noClip = false, isInWeb = false, isAirBorne = false;
    float fallDistance = 0.0f, jumpMovementFactor = 0.02f, stepHeight = 0.6f;
    float width = 0.6f, height = 1.8f;

    mutable glm::vec<3, double> position{0,0,0};
    mutable glm::vec<3, double> lastPos{0,0,0};
    mutable glm::vec<3, double> velocity{0,0,0};

    // Rotation
    Rotation rotation{0.0, 0.0};
    Rotation lastRot{0.0, 0.0};

    bool isCollidedHorizontally = false;
    bool isCollidedVertically = false;
    bool isCollided = false;

    // Jumping
    int jumpTicks = 0;
    bool isJumping = false;

    // Movement input
    double moveForward = 0.0;
    double moveStrafe = 0.0;
    bool sneaking = false;
    bool sprinting = false;

    // Virtual overrides
    [[nodiscard]] virtual float getAIMoveSpeed() const { return 1.0f; }
    [[nodiscard]] virtual double getEyeHeight() const { return 1.62; }
    [[nodiscard]] virtual float jumpHeight() const { return 0.42f; }

    [[nodiscard]] virtual bool isOnLadder() const { return false; }
    [[nodiscard]] virtual bool isInLava() const { return false; }
    [[nodiscard]] virtual bool isInWater() const { return false; }

    void setSneaking(bool sneak) { sneaking = sneak; }
    bool isSneaking() const { return sneaking; }

    void setSprinting(bool sprint) { sprinting = sprint; }
    bool isSprinting() const { return sprinting; }

protected:
    mutable AABB boundingBox{};
};

#endif
