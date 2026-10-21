#pragma once

#include "Entity.hpp"
#include "net/lily/minecpp/net/packets/play/server/EntityMetadata.hpp"

class EntityMob final : public Entity {
public:
    EntityMob(const Minecraft* mc, uint8_t mobType, double x, double y, double z);

    void update() override;
    void setRemotePosition(double x, double y, double z, float yaw, float pitch, int increments);

    uint8_t mobType;
    float headYaw{};
    float previousHeadYaw{};
    EntityMetadata metadata{};
    float limbSwing{};
    float limbSwingAmount{};
    float previousLimbSwingAmount{};

    [[nodiscard]] EntityType getType() const override { return MOB; }
    [[nodiscard]] bool isChild() const;
    [[nodiscard]] bool isSmallArmorStand() const;
    [[nodiscard]] bool isInvisible() const;
    [[nodiscard]] bool hasArmorStandMarker() const;
    [[nodiscard]] bool hasVisibleCustomName() const;
    [[nodiscard]] const std::string& customName() const;
    [[nodiscard]] uint8_t armorStandFlags() const;
    void refreshArmorStandDimensions();
    [[nodiscard]] float shadowSize() const;

private:
    glm::dvec3 targetPosition{};
    Rotation targetRotation{};
    int interpolationSteps{};
};
