#pragma once

#include "AbstractClientPlayer.hpp"

class EntityPlayerSP final : public AbstractClientPlayer {
public:
    explicit EntityPlayerSP(const Minecraft* mc, const float x, const float y, const float z)
        : AbstractClientPlayer(mc, x, y, z) {}

    void update() override;
    void moveEntityWithHeading(float strafe, float forward) override;

    void onUpdateWalkingPlayer();
    void updateEntityActionState();
    void onLivingUpdate() override;
    void prepareMouseLook();
    void handleMouseLook() const;
    void setPlayerSPHealth(float health);
    void resetAfterRespawn();
    void setSprinting(bool sprinting) override;

    [[nodiscard]] bool isUser() const { return true; }

    mutable glm::vec<3, double> lastReportedPos{0, 0, 0};
    mutable Rotation lastReportedRot{0, 0};
    int positionUpdateTicks = 0;
    bool serverSneakState = false;
    bool serverSprintState = false;
    bool hasValidHealth = false;
    int sprintToggleTimer = 0;
    int flyToggleTimer = 0;
    void sendPlayerAbilities() const;
    int sprintingTicksLeft = 0;
    float renderArmYaw = 0.0f;
    float renderArmPitch = 0.0f;
    float prevRenderArmYaw = 0.0f;
    float prevRenderArmPitch = 0.0f;
    bool mouseLookPrepared = false;
};
