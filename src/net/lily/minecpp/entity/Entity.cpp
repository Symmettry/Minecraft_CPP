#include "Entity.hpp"
#include "net/lily/minecpp/Minecraft.hpp"
#include <cmath>

Entity::Entity(const Minecraft* mc, double x, double y, double z)
    : mc(mc), position(x, y, z), lastPos(x, y, z) {}

double Entity::getEyePos() const {
    return position.y + getEyeHeight();
}

double Entity::getLastEyePos() const {
    return lastPos.y + getEyeHeight();
}

void Entity::update() {
    lastPos = position;
    lastRot = rotation;
    onLivingUpdate();
}

void Entity::jump() {
    velocity.y = jumpHeight();
    if (isSprinting() && moveForward > 0.0) {
        double yawRad = rotation.yaw * M_PI / 180.0;
        double boost = 0.2;
        velocity.x += -std::sin(yawRad) * boost;
        velocity.z += std::cos(yawRad) * boost;
    }
    isAirBorne = true;
}

void Entity::onLivingUpdate() {
    if (jumpTicks > 0) --jumpTicks;
    moveStrafe *= 0.98;
    moveForward *= 0.98;

    if (isJumping && onGround && jumpTicks == 0) {
        jump();
        jumpTicks = 10;
    }

    moveEntityWithHeading(moveStrafe, moveForward);

    if (std::abs(velocity.x) < 1e-5) velocity.x = 0.0;
    if (std::abs(velocity.y) < 1e-5) velocity.y = 0.0;
    if (std::abs(velocity.z) < 1e-5) velocity.z = 0.0;
}

void Entity::moveEntityWithHeading(double strafe, double forward) {
    if (isSneaking()) {
        strafe *= 0.3;
        forward *= 0.3;
    }

    double f4 = 0.91;
    if (onGround) {
        const auto* below = mc->world->getBlockAt(
            std::floor(position.x),
            std::floor(getBoundingBox().minY - 1.0),
            std::floor(position.z)
        );
        f4 = below ? below->slipperiness() * 0.91 : 0.546;
    }

    double moveFactor = onGround ? getAIMoveSpeed() * 0.16277136 / (f4 * f4 * f4) : jumpMovementFactor;
    if (isSprinting()) moveFactor *= 1.3;

    moveFlying(strafe, forward, moveFactor);

    if (isOnLadder()) {
        constexpr double ladderClamp = 0.15;
        velocity.x = std::clamp(velocity.x, -ladderClamp, ladderClamp);
        velocity.z = std::clamp(velocity.z, -ladderClamp, ladderClamp);
        if (velocity.y < -ladderClamp) velocity.y = -ladderClamp;
        if (isSneaking() && velocity.y < 0.0) velocity.y = 0.0;
        fallDistance = 0.0;
    }

    moveEntity(velocity.x, velocity.y, velocity.z);

    if (!isInWater() && !isInLava()) {
        velocity.x *= f4;
        velocity.z *= f4;
    }

    if (isInWater() || isInLava()) {
        constexpr double liquidFriction = 0.8;
        moveFlying(strafe, forward, 0.02);
        moveEntity(velocity.x, velocity.y, velocity.z);
        velocity.x *= liquidFriction;
        velocity.y *= liquidFriction;
        velocity.z *= liquidFriction;
        velocity.y -= 0.02;
    }

    velocity.y -= 0.08;
    velocity.y *= 0.98;
}

void Entity::moveFlying(double strafe, double forward, double friction) {
    double f = std::sqrt(strafe * strafe + forward * forward);
    if (f < 1e-4) return;
    if (f > 1.0) {
        strafe /= f;
        forward /= f;
    }
    strafe *= friction;
    forward *= friction;

    double yawRad = rotation.yaw * M_PI / 180.0;
    velocity.x += forward * std::cos(yawRad) - strafe * std::sin(yawRad);
    velocity.z += forward * std::sin(yawRad) + strafe * std::cos(yawRad);
}

void Entity::moveEntity(double dx, double dy, double dz) {
    if (noClip) {
        boundingBox = getBoundingBox().offset(dx, dy, dz);
        resetPositionToBB();
        return;
    }

    double origX = dx, origY = dy, origZ = dz;
    AABB bb = getBoundingBox();
    auto collidingBlocks = mc->world->getCollidingBlocks(bb.offset(dx, dy, dz));

    // Y-axis
    for (const auto& b : collidingBlocks) dy = b->calculateYOffset(bb, dy);
    bb = bb.offset(0.0, dy, 0.0);

    // X-axis
    for (const auto& b : collidingBlocks) dx = b->calculateXOffset(bb, dx);
    bb = bb.offset(dx, 0.0, 0.0);

    // Z-axis
    for (const auto& b : collidingBlocks) dz = b->calculateZOffset(bb, dz);
    bb = bb.offset(0.0, 0.0, dz);

    // StepHeight
    if (stepHeight > 0.0 && onGround && (origX != dx || origZ != dz)) {
        double stepY = stepHeight;
        AABB bbStep = getBoundingBox().offset(origX, stepY, origZ);
        auto stepBlocks = mc->world->getCollidingBlocks(bbStep);
        for (const auto& b : stepBlocks) stepY = b->calculateYOffset(bbStep, stepY);
        bbStep = bbStep.offset(0.0, stepY, 0.0);

        double stepX = origX, stepZ = origZ;
        for (const auto& b : stepBlocks) stepX = b->calculateXOffset(bbStep, stepX);
        bbStep = bbStep.offset(stepX, 0.0, 0.0);
        for (const auto& b : stepBlocks) stepZ = b->calculateZOffset(bbStep, stepZ);
        bbStep = bbStep.offset(0.0, 0.0, stepZ);

        if (stepX*stepX + stepZ*stepZ > dx*dx + dz*dz) {
            dx = stepX;
            dz = stepZ;
            dy = stepY;
            bb = bbStep;
        }
    }

    boundingBox = bb;
    resetPositionToBB();

    isCollidedHorizontally = origX != dx || origZ != dz;
    isCollidedVertically = origY != dy;
    onGround = isCollidedVertically && origY < 0.0;
    isCollided = isCollidedHorizontally || isCollidedVertically;

    if (origX != dx) velocity.x = 0.0;
    if (origY != dy) velocity.y = 0.0;
    if (origZ != dz) velocity.z = 0.0;
}

void Entity::resetPositionToBB() const {
    position.x = (boundingBox.minX + boundingBox.maxX) / 2;
    position.y = boundingBox.minY;
    position.z = (boundingBox.minZ + boundingBox.maxZ) / 2;
}

AABB Entity::getBoundingBox() const {
    return AABB{
        position.x - width/2, position.y, position.z - width/2,
        position.x + width/2, position.y + height, position.z + width/2
    };
}

void Entity::setPosition(double x, double y, double z) const {
    position.x = x;
    position.y = y;
    position.z = z;
    lastPos = position;
    velocity = {0.0, 0.0, 0.0};
}
