#include "EntityPlayer.hpp"
#include "net/lily/minecpp/Minecraft.hpp"
#include <cmath>

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
}

void EntityPlayer::moveEntityWithHeading(double strafe, double forward) {
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