#pragma once
#ifndef ENTITY_HPP

#include <memory>
#include <glm/glm.hpp>

#include "net/lily/minecpp/util/AABB.hpp"
#include "net/lily/minecpp/util/Rotation.hpp"

struct Minecraft;

class Entity : public std::enable_shared_from_this<Entity> {
public:
    const Minecraft* mc = nullptr;

    explicit Entity(const Minecraft* mc, const float x, const float y, const float z) : mc(mc), position(x, y, z) {}
    virtual ~Entity() = default;

    float getEyePos() const {
        return position.y + getEyeHeight();
    }
    float getLastEyePos() const {
        return lastPos.y + getEyeHeight();
    }

    bool onGround = true, noClip = false, isInWeb = false, isAirBorne = false;
    float fallDistance = 0.0f, jumpMovementFactor = 0.02f, stepHeight = 0.6f;

    glm::vec3 position;
    glm::vec3 lastPos{0,0,0};
    glm::vec3 velocity{0,0,0};

    virtual void update();

    [[nodiscard]] virtual float getAIMoveSpeed() const {
        return 1.0f;
    }

    [[nodiscard]] bool isOnLadder() const {
        return false; // todo
    }
    [[nodiscard]] bool isInLava() const {
        return false; // todo
    }
    [[nodiscard]] bool isInWater() const {
        return false; // todo
    }

    void setSneaking(const bool sneak) {
        sneaking = sneak;
    }
    bool isSneaking() const {
        return sneaking;
    }
    void setSprinting(const bool sprint) {
        sprinting = sprint;
    }
    bool isSprinting() const {
        return sprinting;
    }

    virtual void moveEntityWithHeading(float strafe, float forward);

    void resetPositionToBB() {
        position.x = (boundingBox.maxX - boundingBox.minX)/2 + boundingBox.minX;
        position.y = (boundingBox.maxY - boundingBox.minY)/2 + boundingBox.minY;
        position.z = (boundingBox.maxZ - boundingBox.minZ)/2 + boundingBox.minZ;
    }

    virtual void moveEntity(float dx, float dy, float dz);

    void moveFlying(float strafe, float forward, float friction);

    virtual float getEyeHeight() const;
    virtual float jumpHeight() const;

    int jumpTicks = 0;
    bool isJumping = false;

    void jump();

    void onLivingUpdate();

    Rotation rotation{0, 0};
    Rotation lastRot{0, 0};

    float width{}, height{};
    bool isCollidedHorizontally{}, isCollidedVertically{};

    AABB getBoundingBox() {
        boundingBox.minX = position.x - width/2;
        boundingBox.minY = position.y;
        boundingBox.minZ = position.z - width/2;
        boundingBox.maxX = position.x + width/2;
        boundingBox.maxY = position.y + height;
        boundingBox.maxZ = position.z + width/2;
        return boundingBox;
    }

    void setPosition(const float x, const float y, const float z) {
        position.x = x;
        position.y = y;
        position.z = z;
        lastPos.x = x;
        lastPos.y = y;
        lastPos.z = z;
    }

protected:

    void applyFrictionAndGravity();
    float moveForward{}, moveStrafe{};

private:
    /** private because this should never be used without getBoundingBox() */
    AABB boundingBox{};

    bool sneaking = false, sprinting = false;
};

#endif
