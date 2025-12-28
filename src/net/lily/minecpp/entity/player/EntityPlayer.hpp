#pragma once
#ifndef PLAYER_HPP

#include "../Entity.hpp"

class EntityPlayer : public Entity {
public:

    explicit EntityPlayer(const Minecraft* mc, const float x, const float y, const float z) : Entity(mc, x, y, z) {
        width = 0.6f;
    }

    double getEyeHeight() const override {
        return isSneaking() ? 1.54f : 1.62f;
    }

    void update() override;

    void moveEntityWithHeading(float strafe, float forward) override;

    void handleMouseLook();
    void handleSprintToggle();
    void handleKeyboardInput();

    float getAIMoveSpeed() const override {
        return 0.1f; // move speed attribute; todo
    }

    mutable glm::vec<3, double> lastReportedPos{0,0,0};
    mutable Rotation lastReportedRot{0,0};
    int positionUpdateTicks = 0;

    int foodLevel = 20;
    float foodSaturationLevel = 5.0f;
    bool serverSneakState = false, serverSprintState = false;

    std::string username = "lily";
};

#endif