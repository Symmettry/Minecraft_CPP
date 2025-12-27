#include "Entity.hpp"
#include "net/lily/minecpp/Minecraft.hpp"
#include <cmath>
#include <algorithm>

void Entity::update() {
    lastPos = position;
    lastRot = rotation;

    onLivingUpdate();
}

void Entity::moveEntityWithHeading(const float strafe, const float forward) {

    // --- Determine friction ---
    float f4 = 0.91f;
    if (onGround) {
        Block* below = mc->world->getBlockAt(
            std::floor(position.x),
            std::floor(getBoundingBox().minY - 1.0f),
            std::floor(position.z)
        );
        f4 = below->slipperiness() * 0.91f;
    }

    // --- Determine move factor ---
    float moveFactor = onGround ? getAIMoveSpeed() * (0.16277136f / (f4 * f4 * f4)) : jumpMovementFactor;

    // --- Apply sprinting ---
    if (isSprinting()) moveFactor *= 1.3f; // Vanilla sprint multiplier

    // --- Apply movement input ---
    moveFlying(strafe, forward, moveFactor);

    // --- Ladder handling ---
    if (isOnLadder()) {
        constexpr float ladderClamp = 0.15f;
        velocity.x = std::clamp(velocity.x, -ladderClamp, ladderClamp);
        velocity.z = std::clamp(velocity.z, -ladderClamp, ladderClamp);
        if (velocity.y < -ladderClamp) velocity.y = -ladderClamp;
        if (isSneaking() && velocity.y < 0.0f) velocity.y = 0.0f;
        fallDistance = 0.0f;
    }

    // --- Move entity via collisions ---
    moveEntity(velocity.x, velocity.y, velocity.z);

    // --- Ladder vertical boost if stuck ---
    if (isCollidedHorizontally && isOnLadder()) velocity.y = 0.2f;

    // --- Liquid handling ---
    if (isInWater() || isInLava()) {
        constexpr float liquidFactor = 0.5f;
        moveFlying(strafe, forward, 0.02f);
        moveEntity(velocity.x, velocity.y, velocity.z);
        velocity *= liquidFactor;
        velocity.y -= 0.02f; // buoyancy
    }

    // --- Gravity & friction ---
    if (!isInWater() && !isInLava()) {
        constexpr float gravity = 0.08f;
        constexpr float dragY = 0.98f;
        velocity.y -= gravity;
        velocity.x *= f4;
        velocity.z *= f4;
        velocity.y *= dragY;
    }
}

void Entity::moveFlying(float strafe, float forward, float friction) {
    float f = strafe*strafe + forward*forward;
    if (f < 1e-4f) return;

    f = std::sqrt(f);
    if (f < 1.0f) f = 1.0f;
    f = friction / f;

    strafe *= f;
    forward *= f;

    const float yawRad = rotation.yaw * static_cast<float>(M_PI) / 180.0f;
    const float sinYaw = std::sin(yawRad);
    const float cosYaw = std::cos(yawRad);

    velocity.x += forward * cosYaw - strafe * sinYaw;
    velocity.z += forward * sinYaw + strafe * cosYaw;
}

void Entity::moveEntity(float dx, float dy, float dz) {
    if (noClip) {
        boundingBox = boundingBox.offset(dx, dy, dz);
        resetPositionToBB();
        return;
    }

    const AABB originalBox = boundingBox;

    float x = dx, y = dy, z = dz;

    if (isInWeb) {
        isInWeb = false;
        x *= 0.25f;
        y *= 0.05f;
        z *= 0.25f;
        velocity = glm::vec3(0.0f);
    }

    if (onGround && isSneaking() && dynamic_cast<EntityPlayer*>(this)) {
        constexpr float step = 0.05f;
        while (x != 0.0f && mc->world->getCollidingBlocks(boundingBox.offset(x, -1.0f, 0.0f)).empty()) {
            if (std::abs(x) < step) { x = 0.0f; break; }
            x += (x > 0 ? -step : step);
        }
        while (z != 0.0f && mc->world->getCollidingBlocks(boundingBox.offset(0.0f, -1.0f, z)).empty()) {
            if (std::abs(z) < step) { z = 0.0f; break; }
            z += (z > 0 ? -step : step);
        }
        while (x != 0.0f && z != 0.0f && mc->world->getCollidingBlocks(boundingBox.offset(x, -1.0f, z)).empty()) {
            if (std::abs(x) < step) { x = 0.0f; } else x += (x > 0 ? -step : step);
            if (std::abs(z) < step) { z = 0.0f; } else z += (z > 0 ? -step : step);
        }
    }

    // Gather collisions against the original bounding box
    auto collisions = mc->world->getCollidingBlocks(originalBox.offset(x, y, z));

    float moveX = x, moveY = y, moveZ = z;

    // Y-axis sweep
    for (auto &block : collisions) if (block.isOpaque())
        moveY = (moveY > 0 ? std::min(moveY, block.minY() - originalBox.maxY)
                            : std::max(moveY, block.maxY() - originalBox.minY));
    boundingBox = originalBox.offset(0.0f, moveY, 0.0f);

    // Recalculate collisions for X
    collisions = mc->world->getCollidingBlocks(boundingBox.offset(x, 0.0f, 0.0f));
    for (auto &block : collisions) if (block.isOpaque())
        moveX = (moveX > 0 ? std::min(moveX, block.minX() - boundingBox.maxX)
                            : std::max(moveX, block.maxX() - boundingBox.minX));
    boundingBox = boundingBox.offset(moveX, 0.0f, 0.0f);

    // Recalculate collisions for Z
    collisions = mc->world->getCollidingBlocks(boundingBox.offset(0.0f, 0.0f, z));
    for (auto &block : collisions) if (block.isOpaque())
        moveZ = (moveZ > 0 ? std::min(moveZ, block.minZ() - boundingBox.maxZ)
                            : std::max(moveZ, block.maxZ() - boundingBox.minZ));
    boundingBox = boundingBox.offset(0.0f, 0.0f, moveZ);

    // Step handling
    if (stepHeight > 0.0f && onGround && (x != moveX || z != moveZ)) {
        AABB stepBox = originalBox.offset(x, stepHeight, z);
        auto stepCollisions = mc->world->getCollidingBlocks(stepBox);
        float stepY = stepHeight;
        for (auto &block : stepCollisions) if (block.isOpaque())
            stepY = std::min(stepY, block.minY() - originalBox.maxY);

        stepBox = originalBox.offset(x, stepY, z);
        float stepX = x, stepZ = z;
        for (auto &block : stepCollisions) if (block.isOpaque()) {
            stepX = (stepX > 0 ? std::min(stepX, block.minX() - stepBox.maxX)
                                : std::max(stepX, block.maxX() - stepBox.minX));
            stepZ = (stepZ > 0 ? std::min(stepZ, block.minZ() - stepBox.maxZ)
                                : std::max(stepZ, block.maxZ() - stepBox.minZ));
        }

        float distOriginal = moveX*moveX + moveZ*moveZ;
        float distStep = stepX*stepX + stepZ*stepZ;
        if (distStep > distOriginal) {
            moveX = stepX;
            moveY = stepY;
            moveZ = stepZ;
            boundingBox = originalBox.offset(moveX, moveY, moveZ);
        }
    }

    // Final position
    position = glm::vec3(
        (boundingBox.minX + boundingBox.maxX) / 2.0f,
        boundingBox.minY,
        (boundingBox.minZ + boundingBox.maxZ) / 2.0f
    );

    isCollidedHorizontally = x != moveX || z != moveZ;
    isCollidedVertically = y != moveY;
    onGround = isCollidedVertically && y < 0.0f;

    if (isCollidedVertically) velocity.y = 0.0f;
    if (isCollidedHorizontally) {
        if (x != moveX) velocity.x = 0.0f;
        if (z != moveZ) velocity.z = 0.0f;
    }

    // Trigger block collision callbacks todo
    // for (auto &block : collisions) block.onEntityCollidedWithBlock(this);
}

float Entity::jumpHeight() const { return 0.42f; }

float Entity::getEyeHeight() const { return 1.62f; }

void Entity::jump() {
    velocity.y = jumpHeight();

    // Jump potion effect
    // if (hasPotion(Potion::Jump)) {
    //     velocity.y += (getPotionAmplifier(Potion::Jump) + 1) * 0.1f;
    // }

    // Sprint jump horizontal boost
    if (isSprinting() && moveForward > 0.0f) {
        const float yawRad = rotation.yaw * static_cast<float>(M_PI / 180.0f);
        constexpr float sprintBoost = 0.2f;
        velocity.z += std::sin(yawRad) * sprintBoost;
        velocity.x += std::cos(yawRad) * sprintBoost;
    }

    isAirBorne = true;
}

void Entity::onLivingUpdate() {
    if (jumpTicks > 0) --jumpTicks;

    moveStrafe *= 0.98f;
    moveForward *= 0.98f;

    if (isJumping && onGround && jumpTicks == 0) {
        jump();
        jumpTicks = 10;
    }

    moveEntityWithHeading(moveStrafe, moveForward);

    if (std::abs(velocity.x) < 0.005f) velocity.x = 0.0f;
    if (std::abs(velocity.y) < 0.005f) velocity.y = 0.0f;
    if (std::abs(velocity.z) < 0.005f) velocity.z = 0.0f;
}
