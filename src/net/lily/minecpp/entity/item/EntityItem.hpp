#pragma once

#include "net/lily/minecpp/entity/Entity.hpp"

class EntityItem final : public Entity {
public:
    explicit EntityItem(const Minecraft* mc, double x, double y, double z);

    void update() override;

    EntityType getType() const override { return ITEM; }

    int16_t itemId = -1;
    int8_t count = 0;
    int16_t damage = 0;
    bool enchanted = false;
    int age = 0;
    float hoverStart = 0.0f;
    bool collecting = false;
    int collectionAge = 0;
    glm::dvec3 collectionStart{};
    std::weak_ptr<Entity> collector;

    void beginCollection(const std::shared_ptr<Entity>& target);
};
