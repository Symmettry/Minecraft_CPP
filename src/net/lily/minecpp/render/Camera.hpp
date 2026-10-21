#pragma once

#ifndef CAMERA_H
#define CAMERA_H

#include <utility>
#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "../entity/Entity.hpp"

struct Camera {
    void setRenderViewEntity(const std::shared_ptr<Entity>& oe) {
        entity = oe;
    }

    Camera(const float x, const float y, const float z, std::shared_ptr<Entity> entity) : x(x), y(y), z(z), entity(std::move(entity)) { }

    [[nodiscard]] double getX(const double pt) const {
        return x + std::lerp(entity->lastPos.x, entity->position.x, pt);
    }
    [[nodiscard]] double getY(const double pt) const {
        return y + std::lerp(entity->getLastEyePos(), entity->getEyePos(), pt);
    }
    [[nodiscard]] double getZ(const double pt) const {
        return z + std::lerp(entity->lastPos.z, entity->position.z, pt);
    }

    [[nodiscard]] float getRotX(const double pt) const {
        return std::lerp(entity->lastRot.pitch, entity->rotation.pitch, pt);
    }
    [[nodiscard]] float getRotY(const double pt) const {
        return std::lerp(entity->lastRot.yaw, entity->rotation.yaw, static_cast<float>(pt));
    }

    [[nodiscard]] std::shared_ptr<Entity>& getRenderViewEntity() {
        return entity;
    }


private:
    float x, y, z;
    std::shared_ptr<Entity> entity;
};

#endif
