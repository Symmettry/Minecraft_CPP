#include "EntityOtherPlayerMP.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/render/Renderer.hpp"
#include "net/lily/minecpp/world/World.hpp"

namespace {
float wrapDegrees(float angle) {
    angle = std::fmod(angle, 360.0f);
    if (angle >= 180.0f) angle -= 360.0f;
    if (angle < -180.0f) angle += 360.0f;
    return angle;
}
}

bool EntityOtherPlayerMP::usesSlimSkin() const {
    if (!skinUrl.empty()) return slimSkin;
    uint32_t hash = 0;
    for (size_t offset = 0; offset < uuid.size(); offset += 4) {
        hash ^= static_cast<uint32_t>(uuid[offset]) << 24U
            | static_cast<uint32_t>(uuid[offset + 1]) << 16U
            | static_cast<uint32_t>(uuid[offset + 2]) << 8U
            | static_cast<uint32_t>(uuid[offset + 3]);
    }
    return (hash & 1U) != 0;
}

void EntityOtherPlayerMP::setRemotePosition(const double x, const double y, const double z,
                                            const float yaw, const float pitch,
                                            const int increments) {
    targetPosition = {x, y, z};
    targetRotation = {yaw, pitch};
    interpolationSteps = increments;
}

void EntityOtherPlayerMP::update() {
    const bool riding = updateRidden();
    if (!riding) lastPos = position;
    lastRot = rotation;
    prevRenderYawOffset = renderYawOffset;
    prevRotationYawHead = rotationYawHead;
    prevLimbSwingAmount = limbSwingAmount;
    prevSwingProgress = swingProgress;
    if (hurtTime > 0) --hurtTime;
    if (hurtResistantTime > 0) --hurtResistantTime;
    if (riding) {
        interpolationSteps = 0;
        limbSwingAmount *= 0.8f;
        limbSwing += limbSwingAmount;
        renderYawOffset = rotation.yaw;
        rotationYawHead = rotationYawHead; // head yaw still comes from the head-look packets
        return;
    }

    if (interpolationSteps > 0) {
        const double step = interpolationSteps;
        const glm::dvec3 next = position + (targetPosition - position) / step;
        float yawDifference = targetRotation.yaw - rotation.yaw;
        while (yawDifference < -180.0f) yawDifference += 360.0f;
        while (yawDifference >= 180.0f) yawDifference -= 360.0f;
        setPositionPreservingPrevious(next.x, next.y, next.z);
        rotation.yaw += yawDifference / static_cast<float>(step);
        rotation.pitch += (targetRotation.pitch - rotation.pitch) / static_cast<float>(step);
        --interpolationSteps;
    }

    const double dx = position.x - lastPos.x;
    const double dy = position.y - lastPos.y;
    const double dz = position.z - lastPos.z;
    const double distanceSquared = dx * dx + dz * dz;
    const float movement = std::min(static_cast<float>(std::sqrt(distanceSquared) * 4.0), 1.0f);
    limbSwingAmount += (movement - limbSwingAmount) * 0.4f;
    limbSwing += limbSwingAmount;

    if (!onGround && dy < 0.0) {
        fallDistance -= static_cast<float>(dy);
    } else if (onGround && !previousOnGround) {
        if (fallDistance > 3.0f) {
            const int blockX = static_cast<int>(std::floor(position.x));
            const int blockY = static_cast<int>(std::floor(position.y - 0.2));
            const int blockZ = static_cast<int>(std::floor(position.z));
            const Block landedOn = mc->world->getBlockAt(blockX, blockY, blockZ);
            if (blockId(landedOn) != blockId(BLOCK_AIR)) {
                const float strength = std::min(0.2f + std::ceil(fallDistance - 3.0f) / 15.0f, 2.5f);
                mc->renderer->addBlockLandingParticles(position, width, landedOn,
                    static_cast<int>(150.0f * strength));
            }
        }
        fallDistance = 0.0f;
    }
    previousOnGround = onGround;

    if (isSprinting() && !isInWater() && distanceSquared > 1.0E-5) {
        const int blockX = static_cast<int>(std::floor(position.x));
        const int blockY = static_cast<int>(std::floor(position.y - 0.2));
        const int blockZ = static_cast<int>(std::floor(position.z));
        mc->renderer->addBlockRunningParticle({position.x, position.y, position.z},
            {dx, dy, dz}, width, mc->world->getBlockAt(blockX, blockY, blockZ));
    }

    float targetBodyYaw = renderYawOffset;
    if (distanceSquared > 0.0025000002) {
        targetBodyYaw = static_cast<float>(std::atan2(dz, dx) * 180.0 / M_PI - 90.0);
    }
    if (swingProgress > 0.0f) targetBodyYaw = rotation.yaw;
    renderYawOffset += wrapDegrees(targetBodyYaw - renderYawOffset) * 0.3f;
    float headBodyDelta = wrapDegrees(rotation.yaw - renderYawOffset);
    limbSwingDirection = headBodyDelta < -90.0f || headBodyDelta >= 90.0f ? -1.0f : 1.0f;
    headBodyDelta = std::clamp(headBodyDelta, -75.0f, 75.0f);
    renderYawOffset = rotation.yaw - headBodyDelta;
    if (headBodyDelta * headBodyDelta > 2500.0f) renderYawOffset += headBodyDelta * 0.2f;
    while (renderYawOffset - prevRenderYawOffset < -180.0f) prevRenderYawOffset -= 360.0f;
    while (renderYawOffset - prevRenderYawOffset >= 180.0f) prevRenderYawOffset += 360.0f;
    while (rotationYawHead - prevRotationYawHead < -180.0f) prevRotationYawHead -= 360.0f;
    while (rotationYawHead - prevRotationYawHead >= 180.0f) prevRotationYawHead += 360.0f;
    updateArmSwingProgress();
}
