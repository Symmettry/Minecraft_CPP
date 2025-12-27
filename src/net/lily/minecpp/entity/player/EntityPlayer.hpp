#pragma once
#ifndef PLAYER_HPP

#include "../Entity.hpp"

class EntityPlayer : public Entity {
public:

    explicit EntityPlayer(const Minecraft* mc, const float x, const float y, const float z) : Entity(mc, x, y, z) {}

    float getEyeHeight() const override {
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
};

#endif