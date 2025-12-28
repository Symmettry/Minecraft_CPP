#pragma once

#ifndef CAMERA_H
#define CAMERA_H

#include <utility>

#include "../entity/Entity.hpp"

struct Camera {
    void setRenderViewEntity(const std::shared_ptr<Entity>& oe) {
        entity = oe;
    }

    Camera(const float x, const float y, const float z, std::shared_ptr<Entity> entity) : x(x), y(y), z(z), entity(std::move(entity)) { }

    [[nodiscard]] float getX(const float pt) const {
        return x + std::lerp(entity->lastPos.x, entity->position.x, pt);
    }
    [[nodiscard]] float getY(const float pt) const {
        return y + std::lerp(entity->getLastEyePos(), entity->getEyePos(), pt) + 0.5f;
    }
    [[nodiscard]] float getZ(const float pt) const {
        return z + std::lerp(entity->lastPos.z, entity->position.z, pt);
    }

    [[nodiscard]] float getRotX(const float pt) const {
        return std::lerp(entity->lastRot.pitch, entity->rotation.pitch, pt);
    }
    [[nodiscard]] float getRotY(const float pt) const {
        return std::lerp(entity->lastRot.yaw, entity->rotation.yaw, pt);
    }

    [[nodiscard]] std::shared_ptr<Entity>& getRenderViewEntity() {
        return entity;
    }

private:
    float x, y, z;
    std::shared_ptr<Entity> entity;
};

#endif