#pragma once
#ifndef BLOCK_HPP
#define BLOCK_HPP

#include <algorithm>
#include <glm/vec3.hpp>

#include "../../util/AABB.hpp"

enum class BlockType {
    Air,
    Dirt,
    Grass,
    Stone
};

struct Block {
    BlockType type;
    glm::vec3 position;

    Block() : type(BlockType::Air), position(0,0,0) {}
    Block(const int x, const int y, const int z) : type(BlockType::Air), position(x, y, z) {}
    explicit Block(const BlockType t, const int x, const int y, const int z) : type(t), position(x, y, z) {}

    [[nodiscard]] bool isOpaque() const {
        return type != BlockType::Air;
    }

    [[nodiscard]] AABB getBoundingBox() const {
        return AABB(minX(), minY(), minZ(), maxX(), maxY(), maxZ());
    }

    [[nodiscard]] float minX() const { return position.x; }
    [[nodiscard]] float minY() const { return position.y; }
    [[nodiscard]] float minZ() const { return position.z; }
    [[nodiscard]] float maxX() const { return position.x + 1; }
    [[nodiscard]] float maxY() const { return position.y + 1; }
    [[nodiscard]] float maxZ() const { return position.z + 1; }

    [[nodiscard]] float calculateXOffset(const AABB &other, float dx) const;
    [[nodiscard]] float calculateYOffset(const AABB &other, float dy) const;
    [[nodiscard]] float calculateZOffset(const AABB &other, float dz) const;

    float slipperiness() {
        return 0.6f;
    }
};

#endif
