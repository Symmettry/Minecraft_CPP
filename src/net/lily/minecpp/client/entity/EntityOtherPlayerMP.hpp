#pragma once

#include <array>

#include "net/lily/minecpp/entity/player/EntityPlayer.hpp"

class EntityOtherPlayerMP final : public EntityPlayer {
public:
    explicit EntityOtherPlayerMP(const Minecraft* mc, const double x, const double y, const double z)
        : EntityPlayer(mc, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)) {}

    void update() override;
    void setRemotePosition(double x, double y, double z, float yaw, float pitch, int increments);
    [[nodiscard]] bool usesSlimSkin() const;

    std::array<uint8_t, 16> uuid{};

private:
    glm::dvec3 targetPosition{};
    Rotation targetRotation{};
    int interpolationSteps = 0;
    bool previousOnGround = false;
};
