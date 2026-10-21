#include "EntityMob.hpp"

#include <algorithm>
#include <cmath>

EntityMob::EntityMob(const Minecraft* mc, const uint8_t mobType,
                     const double x, const double y, const double z)
    : Entity(mc, x, y, z), mobType(mobType) {
    switch (mobType) {
        case 30: width = 0.5f; height = 1.975f; break; // armor stand
        case 210: width = 0.98f; height = 0.7f; break; // minecart
        case 58: width = 0.6f; height = 2.9f; break; // enderman
        case 62: width = 0.6f; height = 1.8f; break; // magma cube
        case 65: width = 0.5f; height = 0.9f; break; // bat
        case 68: width = 0.85f; height = 0.85f; break; // guardian
        case 90: width = 0.9f; height = 0.9f; break;
        case 91: case 92: case 96: width = 0.9f; height = 1.3f; break;
        case 93: width = 0.4f; height = 0.7f; break;
        case 94: width = 0.8f; height = 0.8f; break;
        case 95: width = 0.6f; height = 0.8f; break;
        case 98: width = 0.6f; height = 0.7f; break;
        case 99: width = 1.4f; height = 2.9f; break;
        case 100: width = 1.4f; height = 1.6f; break;
        case 101: width = 0.6f; height = 0.7f; break;
        case 120: width = 0.6f; height = 1.8f; break;
        default: width = 0.6f; height = 1.8f; break;
    }
    setPosition(x, y, z);
}

void EntityMob::setRemotePosition(const double x, const double y, const double z,
                                  const float yaw, const float pitch, const int increments) {
    targetPosition = {x, y, z};
    targetRotation = {yaw, pitch};
    interpolationSteps = increments;
}

void EntityMob::update() {
    if (updateRidden()) {
        lastRot = rotation;
        previousHeadYaw = headYaw;
        if (hurtTime > 0) --hurtTime;
        if (hurtResistantTime > 0) --hurtResistantTime;
        return;
    }
    lastPos = position;
    lastRot = rotation;
    previousHeadYaw = headYaw;
    previousLimbSwingAmount = limbSwingAmount;
    if (hurtTime > 0) --hurtTime;
    if (hurtResistantTime > 0) --hurtResistantTime;
    if (interpolationSteps <= 0) {
        limbSwingAmount *= 0.8f;
        limbSwing += limbSwingAmount;
        return;
    }

    const double steps = interpolationSteps;
    const glm::dvec3 next = position + (targetPosition - position) / steps;
    float yawDifference = targetRotation.yaw - rotation.yaw;
    while (yawDifference < -180.0f) yawDifference += 360.0f;
    while (yawDifference >= 180.0f) yawDifference -= 360.0f;
    setPositionPreservingPrevious(next.x, next.y, next.z);
    rotation.yaw += yawDifference / static_cast<float>(steps);
    rotation.pitch += (targetRotation.pitch - rotation.pitch) / static_cast<float>(steps);
    --interpolationSteps;

    const double dx = position.x - lastPos.x;
    const double dz = position.z - lastPos.z;
    if (mobType == 210 && dx * dx + dz * dz > 1.0e-6)
        rotation.yaw = static_cast<float>(std::atan2(dz, dx) * 180.0 / 3.14159265358979); // along the track
    const float movement = std::min(static_cast<float>(std::sqrt(dx * dx + dz * dz) * 4.0), 1.0f);
    limbSwingAmount += (movement - limbSwingAmount) * 0.4f;
    limbSwing += limbSwingAmount;
}

bool EntityMob::isChild() const {
    return mobType != 30 && metadata.bytes[12].has_value() && metadata.bytes[12].value() < 0;
}

uint8_t EntityMob::armorStandFlags() const {
    return mobType == 30 ? static_cast<uint8_t>(metadata.bytes[10].value_or(0)) : 0;
}

bool EntityMob::isSmallArmorStand() const {
    return (armorStandFlags() & 0x01U) != 0;
}

bool EntityMob::isInvisible() const {
    return (metadata.flags & 0x20U) != 0;
}

bool EntityMob::hasArmorStandMarker() const {
    return mobType == 30 && (armorStandFlags() & 0x10U) != 0;
}

bool EntityMob::hasVisibleCustomName() const {
    return mobType == 30 && metadata.bytes[3].value_or(0) == 1
        && !customName().empty();
}

const std::string& EntityMob::customName() const {
    static const std::string empty;
    return metadata.strings[2].has_value() ? metadata.strings[2].value() : empty;
}

void EntityMob::refreshArmorStandDimensions() {
    if (mobType != 30) return;
    if (hasArmorStandMarker()) {
        width = 0.0f;
        height = 0.0f;
    } else if (isSmallArmorStand()) {
        width = 0.25f;
        height = 0.9875f;
    } else {
        width = 0.5f;
        height = 1.975f;
    }
    setPosition(position.x, position.y, position.z);
}

float EntityMob::shadowSize() const {
    float size;
    switch (mobType) {
        case 30: size = 0.0f; break;
        case 90: case 91: case 92: case 96: size = 0.7f; break;
        case 93: case 95: case 98: case 101: size = 0.3f; break;
        case 94: size = 0.7f; break;
        case 55: size = 0.25f * static_cast<float>(std::max<int>(metadata.bytes[16].value_or(1), 1)); break;
        case 99: case 100: size = 0.8f; break;
        default: size = width * 0.75f; break;
    }
    return isChild() ? size * 0.5f : size;
}
