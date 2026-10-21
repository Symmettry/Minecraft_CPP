#pragma once

#include <array>
#include <cstdint>
#include <glm/glm.hpp>

#include "AABB.hpp"

class Entity;

class MovingObjectPosition final {
public:

    enum MovingObjectType {
        MISS,
        BLOCK,
        ENTITY,
    };

    glm::vec<3, int> blockPos{};
    glm::vec<3, double> hitVec{};

    MovingObjectType typeOfHit{MISS};
    uint8_t sideHit = 0;
    const Entity* entityHit{nullptr};
    std::array<AABB, 5> blockBounds{};
    uint8_t blockBoundsCount = 0;

    bool valid = false;

    void clear() {
        blockPos = {};
        hitVec = {};
        typeOfHit = MISS;
        sideHit = 0;
        entityHit = nullptr;
        blockBoundsCount = 0;
        valid = false;
    }

};
