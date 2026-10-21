#include "EntityItem.hpp"
#include "net/lily/minecpp/Minecraft.hpp"

#include <numbers>
#include <random>

EntityItem::EntityItem(const Minecraft* mc, const double x, const double y, const double z)
    : Entity(mc, x, y, z) {
    thread_local std::mt19937 random(std::random_device{}());
    thread_local std::uniform_real_distribution<float> phase(
        0.0f, std::numbers::pi_v<float> * 2.0f);
    hoverStart = phase(random);
    width = 0.25f;
    height = 0.25f;
    setPosition(x, y, z);
}

void EntityItem::update() {
    if (collecting) {
        if (++collectionAge >= 3) mc->world->removeEntity(entityId);
        return;
    }
    lastPos = position;
    velocity.y -= 0.03999999910593033;
    moveEntity(velocity.x, velocity.y, velocity.z);

    float friction = 0.98f;
    if (onGround) friction = BlockUtil::getSlipperiness(getBlockBelow()) * 0.98f;
    velocity.x *= friction;
    velocity.y *= 0.9800000190734863;
    velocity.z *= friction;
    if (onGround) velocity.y *= -0.5;
    ++age;
}

void EntityItem::beginCollection(const std::shared_ptr<Entity>& target) {
    if (collecting || target == nullptr) return;
    collecting = true;
    collectionAge = 0;
    collectionStart = position;
    collector = target;
    velocity = {};
}
