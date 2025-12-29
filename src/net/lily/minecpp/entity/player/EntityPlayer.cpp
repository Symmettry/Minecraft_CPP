#include "EntityPlayer.hpp"
#include "net/lily/minecpp/Minecraft.hpp"
#include <cmath>

#include "net/lily/minecpp/net/packets/play/client/C03PacketPlayer.hpp"

void EntityPlayer::update() {
    // fallback for falling out of the world
    if (position.y < -10) {
        setPosition(0, 1, 0);
    }

    lastPos = position;
    lastRot = rotation;

    handleMouseLook();
    handleKeyboardInput();
    handleSprintToggle();

    onLivingUpdate();

    if (sneaking != serverSneakState) {
        if (sneaking) {} // send c0b sneak
        else {} // send c0b unsneak
        serverSneakState = sneaking;
    }
    if (sprinting != serverSprintState) {
        if (sprinting) {} // send c0b sprint
        else {} // send c0b unsprint
        serverSprintState = sprinting;
    }

    if (!isCurrentViewEntity()) return;
    const double d0 = position.x - lastReportedPos.x;
    const double d1 = getBoundingBox().minY - lastReportedPos.y;
    const double d2 = position.z - lastReportedPos.z;
    const double d3 = rotation.yaw - lastReportedRot.yaw;
    const double d4 = rotation.pitch - lastReportedRot.pitch;
    const bool rotChange = d3 != 0.0 || d4 != 0.0;
    bool posChange = d0 * d0 + d1 * d1 + d2 * d2 > 9.0E-4 || positionUpdateTicks >= 20;

    if (/*ridingEntity == null*/ true) {
        if (posChange && rotChange) {
            mc->netClient->sendPacket(C06PacketPlayerPosLook{position.x, getBoundingBox().minY, position.z, rotation.yaw, rotation.pitch, onGround});
        } else if (posChange) {
            mc->netClient->sendPacket(C04PacketPlayerPosition{position.x, getBoundingBox().minY, position.z, onGround});
        } else if (rotChange) {
            mc->netClient->sendPacket(C05PacketPlayerLook{rotation.yaw, rotation.pitch, onGround});
        } else {
            mc->netClient->sendPacket(C03PacketPlayer{onGround});
        }
    } else {
        mc->netClient->sendPacket(C06PacketPlayerPosLook{velocity.x, -999.0, velocity.z, rotation.yaw, rotation.pitch, onGround});
        posChange = false;
    }

    ++positionUpdateTicks;

    if (posChange) {
        lastReportedPos.x = position.x;
        lastReportedPos.y = getBoundingBox().minY;
        lastReportedPos.z = position.z;
        positionUpdateTicks = 0;
    }

    if (rotChange) {
        lastReportedRot.yaw = rotation.yaw;
        lastReportedRot.pitch = rotation.pitch;
    }
}

void EntityPlayer::moveEntityWithHeading(float strafe, float forward) {
    //todo
    // const glm::vec3 prevPos = position;

    // if (capabilities.isFlying && ridingEntity == nullptr) {
    //     float originalJump = jumpMovementFactor;
    //     jumpMovementFactor = capabilities.getFlySpeed() * (isSprinting() ? 2.0f : 1.0f);
    //
    //     Entity::moveEntityWithHeading(strafe, forward);
    //
    //     velocity.y *= 0.6f; // vertical damping
    //     jumpMovementFactor = originalJump;
    // } else {
        Entity::moveEntityWithHeading(strafe, forward);
    // }

    // Update movement stats if needed
    // addMovementStat(position.x - prevPos.x, position.y - prevPos.y, position.z - prevPos.z);
}

void EntityPlayer::handleMouseLook() {
    const float rawDX = mc->input->getMouseDeltaX();
    const float rawDY = mc->input->getMouseDeltaY();

    const float sens = mc->settings->mouseSensitivity.value;
    const float f = sens * 0.6f + 0.2f;
    const float deltaX = rawDX * f * f * f * 8.0f;
    const float deltaY = rawDY * f * f * f * 8.0f;

    rotation.yaw += deltaX;
    rotation.pitch -= deltaY;
    rotation.pitch = std::clamp(rotation.pitch, -90.0f, 90.0f);
}

void EntityPlayer::handleKeyboardInput() {
    moveForward = moveStrafe = 0.0f;

    if (mc->settings->isKeyDown(mc->settings->moveForward)) moveForward += 1.0f;
    if (mc->settings->isKeyDown(mc->settings->moveBackward)) moveForward -= 1.0f;
    if (mc->settings->isKeyDown(mc->settings->moveRight)) moveStrafe += 1.0f;
    if (mc->settings->isKeyDown(mc->settings->moveLeft)) moveStrafe -= 1.0f;

    setSneaking(mc->settings->isKeyDown(mc->settings->sneak));
    isJumping = mc->settings->isKeyDown(mc->settings->jump);
}

void EntityPlayer::handleSprintToggle() {
    if (((!isSneaking() && mc->settings->isKeyDown(mc->settings->sprint)) || (isSprinting() && !isCollidedHorizontally)) && moveForward > 0.0f)
        setSprinting(true);
    else
        setSprinting(false);
}