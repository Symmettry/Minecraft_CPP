#include "EntityPlayerSP.hpp"
#include <cmath>

#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/net/packets/play/client/C03PacketPlayer.hpp"
#include "net/lily/minecpp/net/packets/play/client/C0BPacketEntityAction.hpp"
#include "net/lily/minecpp/net/packets/play/client/C13PacketPlayerAbilities.hpp"
#include "net/lily/minecpp/net/packets/play/client/C0CPacketInput.hpp"

void EntityPlayerSP::update() {
    // entityPlayerSP.onUpdate only advances the local player when its column is loaded. This is especially important between the initial S08 and the arrival/meshing of the spawn chunk: applying gravity against an absent chunk makes the client fall before terrain exists
    if (mc->world == nullptr) return;
    const int blockX = static_cast<int>(std::floor(position.x));
    const int blockZ = static_cast<int>(std::floor(position.z));
    const std::shared_ptr<Chunk> currentChunk = mc->world->getLoadedChunkAt(blockX, blockZ);
    if (currentChunk == nullptr || !currentChunk->dataReady.load(std::memory_order_acquire)) return;

    if (position.y < -10) setPosition(0, 1, 0);

    lastPos = position;
    const bool preparedMouseLook = mouseLookPrepared;
    mouseLookPrepared = false;
    if (!preparedMouseLook) lastRot = rotation;
    prevDistanceWalkedModified = distanceWalkedModified;
    prevCameraYaw = cameraYaw;
    prevCameraPitch = cameraPitch;
    prevFovModifier = fovModifier;
    prevRenderYawOffset = renderYawOffset;
    prevRotationYawHead = rotationYawHead;
    prevLimbSwingAmount = limbSwingAmount;
    movedLastTick = false;
    if (hurtTime > 0) --hurtTime;
    if (hurtResistantTime > 0) --hurtResistantTime;

    if (vehicleId >= 0 && updateRidden()) {
        // entityPlayerSP.onUpdate while riding: follow the vehicle, send look + steering input
        if (!preparedMouseLook) handleMouseLook();
        updateEntityActionState();
        updateArmSwingProgress();
        rotationYawHead = rotation.yaw;
        renderYawOffset = rotation.yaw;
        limbSwingAmount *= 0.8f;
        limbSwing += limbSwingAmount;
        isFlying = false;
        if (mc->netClient != nullptr) {
            mc->netClient->sendPacket(C05PacketPlayerLook{rotation.yaw, rotation.pitch, onGround});
            mc->netClient->sendPacket(C0CPacketInput{moveStrafe, moveForward, isJumping, isSneaking()});
        }
        return;
    }

    handleWaterMovement();
    if (!preparedMouseLook) handleMouseLook();
    onLivingUpdate();
    if (isSprinting() && !isInWater()) {
        const int blockX = static_cast<int>(std::floor(position.x));
        const int blockY = static_cast<int>(std::floor(position.y - 0.20000000298023224));
        const int blockZ = static_cast<int>(std::floor(position.z));
        const Block block = mc->world->getBlockAt(blockX, blockY, blockZ);
        mc->renderer->addBlockRunningParticle(
            {position.x, getBoundingBox().minY, position.z}, velocity, width, block);
    }
    updateArmSwingProgress();
    movedLastTick = lastPos != position;

    const double dx = position.x - lastPos.x;
    const double dz = position.z - lastPos.z;
    const auto wrapDegrees = [](float angle) {
        angle = std::fmod(angle, 360.0f);
        if (angle >= 180.0f) angle -= 360.0f;
        if (angle < -180.0f) angle += 360.0f;
        return angle;
    };
    float targetBodyYaw = renderYawOffset;
    if (dx * dx + dz * dz > 0.0025000002) {
        targetBodyYaw = static_cast<float>(std::atan2(dz, dx) * 180.0 / M_PI - 90.0);
    }
    if (swingProgress > 0.0f) targetBodyYaw = rotation.yaw;
    renderYawOffset += wrapDegrees(targetBodyYaw - renderYawOffset) * 0.3f;
    rotationYawHead = rotation.yaw;
    float headBodyDelta = wrapDegrees(rotationYawHead - renderYawOffset);
    limbSwingDirection = headBodyDelta < -90.0f || headBodyDelta >= 90.0f ? -1.0f : 1.0f;
    headBodyDelta = std::clamp(headBodyDelta, -75.0f, 75.0f);
    renderYawOffset = rotationYawHead - headBodyDelta;
    if (headBodyDelta * headBodyDelta > 2500.0f) renderYawOffset += headBodyDelta * 0.2f;
    while (renderYawOffset - prevRenderYawOffset < -180.0f) prevRenderYawOffset -= 360.0f;
    while (renderYawOffset - prevRenderYawOffset >= 180.0f) prevRenderYawOffset += 360.0f;
    while (rotationYawHead - prevRotationYawHead < -180.0f) prevRotationYawHead -= 360.0f;
    while (rotationYawHead - prevRotationYawHead >= 180.0f) prevRotationYawHead += 360.0f;
    distanceWalkedModified += static_cast<float>(std::sqrt(dx * dx + dz * dz) * 0.6);
    const float targetLimbSwing = std::min(
        static_cast<float>(std::sqrt(dx * dx + dz * dz) * 4.0), 1.0f);
    limbSwingAmount += (targetLimbSwing - limbSwingAmount) * 0.4f;
    limbSwing += limbSwingAmount;
    float horizontalSpeed = static_cast<float>(std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z));
    horizontalSpeed = onGround ? std::min(horizontalSpeed, 0.1f) : 0.0f;
    const float verticalPitch = onGround ? 0.0f
        : static_cast<float>(std::atan(-velocity.y * 0.2) * 15.0);
    cameraYaw += (horizontalSpeed - cameraYaw) * 0.4f;
    cameraPitch += (verticalPitch - cameraPitch) * 0.8f;
    const float targetFovModifier = isSprinting() ? 1.15f : 1.0f;
    fovModifier += (targetFovModifier - fovModifier) * 0.5f;
    firstUpdate = false;
    fovModifier = std::clamp(fovModifier, 0.1f, 1.5f);

    onUpdateWalkingPlayer();
}

void EntityPlayerSP::onUpdateWalkingPlayer() {
    if (mc->netClient == nullptr) return;
    if (isSprinting() != serverSprintState) {
        const auto action = isSprinting()
            ? C0BPacketEntityAction::Action::StartSprinting
            : C0BPacketEntityAction::Action::StopSprinting;
        mc->netClient->sendPacket(C0BPacketEntityAction{entityId, action});
        serverSprintState = isSprinting();
    }

    if (isSneaking() != serverSneakState) {
        const auto action = isSneaking()
            ? C0BPacketEntityAction::Action::StartSneaking
            : C0BPacketEntityAction::Action::StopSneaking;
        mc->netClient->sendPacket(C0BPacketEntityAction{entityId, action});
        serverSneakState = isSneaking();
    }

    if (!isCurrentViewEntity()) return;
    const double dx = position.x - lastReportedPos.x;
    const double dy = getBoundingBox().minY - lastReportedPos.y;
    const double dz = position.z - lastReportedPos.z;
    const double yawDelta = rotation.yaw - lastReportedRot.yaw;
    const double pitchDelta = rotation.pitch - lastReportedRot.pitch;
    const bool rotationChanged = yawDelta != 0.0 || pitchDelta != 0.0;
    bool positionChanged = dx * dx + dy * dy + dz * dz > 9.0E-4
        || positionUpdateTicks >= 20;

    if (ridingEntity == nullptr) {
        if (positionChanged && rotationChanged) {
            mc->netClient->sendPacket(C06PacketPlayerPosLook{position.x, getBoundingBox().minY,
                position.z, rotation.yaw, rotation.pitch, onGround});
        } else if (positionChanged) {
            mc->netClient->sendPacket(C04PacketPlayerPosition{position.x, getBoundingBox().minY,
                position.z, onGround});
        } else if (rotationChanged) {
            mc->netClient->sendPacket(C05PacketPlayerLook{rotation.yaw, rotation.pitch, onGround});
        } else {
            mc->netClient->sendPacket(C03PacketPlayer{onGround});
        }
    } else {
        mc->netClient->sendPacket(C06PacketPlayerPosLook{velocity.x, -999.0, velocity.z,
            rotation.yaw, rotation.pitch, onGround});
        positionChanged = false;
    }

    ++positionUpdateTicks;
    if (positionChanged) {
        lastReportedPos = {position.x, getBoundingBox().minY, position.z};
        positionUpdateTicks = 0;
    }
    if (rotationChanged) lastReportedRot = rotation;
}

void EntityPlayerSP::moveEntityWithHeading(const float strafe, const float forward) {
    if (isFlying) {
        // entityPlayer.moveEntityWithHeading: fly speed replaces air control, gravity is cancelled
        const double verticalVelocity = velocity.y;
        const float airControl = jumpMovementFactor;
        jumpMovementFactor = flySpeed * (isSprinting() ? 2.0f : 1.0f);
        Entity::moveEntityWithHeading(strafe, forward);
        velocity.y = verticalVelocity * 0.6;
        jumpMovementFactor = airControl;
        return;
    }
    Entity::moveEntityWithHeading(strafe, forward);
}

void EntityPlayerSP::sendPlayerAbilities() const {
    if (mc->netClient == nullptr) return;
    mc->netClient->sendPacket(C13PacketPlayerAbilities{
        gameType == GameType::CREATIVE, isFlying, allowFlying, gameType == GameType::CREATIVE,
        flySpeed, 0.1f});
}

void EntityPlayerSP::prepareMouseLook() {
    lastRot = rotation;
    handleMouseLook();
    mouseLookPrepared = true;
}

void EntityPlayerSP::handleMouseLook() const {
    const float rawDX = mc->input->getMouseDeltaX();
    const float rawDY = mc->input->getMouseDeltaY();
    if (rawDX == 0.0f && rawDY == 0.0f) {
        mc->renderer->shouldCalcView = false;
        return;
    }

    mc->renderer->shouldCalcView = true;
    const float sensitivity = mc->settings->mouseSensitivity.value;
    const float multiplier = sensitivity * 0.6f + 0.2f;
    const float scaled = multiplier * multiplier * multiplier * 8.0f * 0.15f;
    const float invert = mc->settings->getBool("invertYMouse") ? -1.0f : 1.0f;
    rotation.yaw += rawDX * scaled;
    rotation.pitch += rawDY * scaled * invert;
    rotation.pitch = std::clamp(rotation.pitch, -90.0f, 90.0f);
}

void EntityPlayerSP::updateEntityActionState() {
    moveForward = moveStrafe = 0.0f;
    if (mc->settings->isKeyDown(mc->settings->moveForward)) moveForward += 1.0f;
    if (mc->settings->isKeyDown(mc->settings->moveBackward)) moveForward -= 1.0f;
    if (mc->settings->isKeyDown(mc->settings->moveRight)) moveStrafe -= 1.0f;
    if (mc->settings->isKeyDown(mc->settings->moveLeft)) moveStrafe += 1.0f;

    setSneaking(mc->settings->isKeyDown(mc->settings->sneak));
    if (isSneaking()) {
        moveStrafe *= 0.3f;
        moveForward *= 0.3f;
    }
    if (usingItem) {
        moveStrafe *= 0.2f;
        moveForward *= 0.2f;
    }
    const bool jumpDown = mc->settings->isKeyDown(mc->settings->jump);
    if (jumpDown && !isJumping) jumpTicks = 0;
    isJumping = jumpDown;

    prevRenderArmYaw = renderArmYaw;
    prevRenderArmPitch = renderArmPitch;
    renderArmPitch += (rotation.pitch - renderArmPitch) * 0.5f;
    renderArmYaw += (rotation.yaw - renderArmYaw) * 0.5f;
}

void EntityPlayerSP::onLivingUpdate() {
    if (sprintingTicksLeft > 0 && --sprintingTicksLeft == 0) setSprinting(false);
    if (sprintToggleTimer > 0) --sprintToggleTimer;
    if (flyToggleTimer > 0) --flyToggleTimer;

    const bool wasJumping = isJumping;
    const bool wasSneaking = isSneaking();
    const bool wasMovingForward = moveForward >= 0.8f;
    updateEntityActionState();

    const bool movingForward = moveForward >= 0.8f;
    const bool canSprint = foodLevel > 6 || gameType == GameType::CREATIVE || isSpectator();
    if (onGround && !wasSneaking && !wasMovingForward && movingForward && !isSprinting() && canSprint) {
        if (sprintToggleTimer <= 0 && !mc->settings->isKeyDown(mc->settings->sprint)) {
            sprintToggleTimer = 7;
        } else {
            setSprinting(true);
        }
    }

    if (!isSprinting() && movingForward && canSprint && mc->settings->isKeyDown(mc->settings->sprint)) {
        setSprinting(true);
    }
    if (isSprinting() && (!movingForward || isCollidedHorizontally || !canSprint)) setSprinting(false);

    if (!wasJumping && isJumping) jumpTicks = 0;

    // entityPlayerSP.onLivingUpdate: double-tapping jump toggles flight, jump/sneak move vertically
    if (allowFlying && !isSpectator() && !wasJumping && isJumping) {
        if (flyToggleTimer == 0) {
            flyToggleTimer = 7;
        } else {
            isFlying = !isFlying;
            sendPlayerAbilities();
            flyToggleTimer = 0;
        }
    }
    if (isFlying) {
        if (isSneaking()) velocity.y -= flySpeed * 3.0f;
        if (isJumping) velocity.y += flySpeed * 3.0f;
        if (onGround && !isSpectator()) {
            isFlying = false;
            sendPlayerAbilities();
        }
    }
    Entity::onLivingUpdate();

    // entityPlayer.onLivingUpdate updates this after travel, so the value is consumed on the following tick. Preserve the double intermediate used by 1.8.9 for sprint air control
    constexpr float speedInAir = 0.02F;
    jumpMovementFactor = speedInAir;
    if (isSprinting()) {
        jumpMovementFactor = static_cast<float>(
            static_cast<double>(jumpMovementFactor) + static_cast<double>(speedInAir) * 0.3);
    }
}

void EntityPlayerSP::setPlayerSPHealth(const float newHealth) {
    if (!hasValidHealth) {
        hasValidHealth = true;
        health = lastHealth = newHealth;
        return;
    }
    lastHealth = health;
    if (newHealth < health) {
        hurtTime = maxHurtTime = 10;
        hurtResistantTime = 20;
        attackedAtYaw = 0.0f;
    } else if (newHealth > health) {
        hurtResistantTime = 10; // entityPlayerSP.setPlayerSPHealth: maxHurtResistantTime / 2, which makes the bar flash
    }
    health = newHealth;
}

void EntityPlayerSP::resetAfterRespawn() {
    velocity = {0.0, 0.0, 0.0};
    moveForward = 0.0f;
    moveStrafe = 0.0f;
    isJumping = false;
    jumpTicks = 0;
    onGround = false;
    isInWeb = false;
    inWater = false;
    fallDistance = 0.0f;
    sprintToggleTimer = 0;
    flyToggleTimer = 0;
    setSprinting(false);
    setSneaking(false);
    serverSprintState = false;
    serverSneakState = false;
    positionUpdateTicks = 0;
    lastReportedPos = {position.x, getBoundingBox().minY, position.z};
    lastReportedRot = rotation;
    firstUpdate = true;
}

void EntityPlayerSP::setSprinting(const bool shouldSprint) {
    Entity::setSprinting(shouldSprint);
    sprintingTicksLeft = shouldSprint ? 600 : 0;
}
