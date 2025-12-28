#pragma once
#ifndef BLOCK_HPP
#define BLOCK_HPP

#include <algorithm>
#include <string>
#include <glm/vec3.hpp>

#include "../../util/AABB.hpp"

enum class Material {
    Air,
    Dirt,
    Grass,
    Stone,
    Obsidian,
    Ice,
};

using BlockPos = glm::vec<3, int>;

class Block {
public:
    virtual ~Block() = default;

    Material material;
    BlockPos position;

    Block() : material(Material::Air), position(0,0,0) {}
    Block(const int x, const int y, const int z) : material(Material::Air), position(x, y, z) {}
    explicit Block(const Material t, const int x, const int y, const int z) : material(t), position(x, y, z) {}

    [[nodiscard]] virtual bool isOpaque() const {
        return false;
    }

    [[nodiscard]] AABB getBoundingBox() const {
        return {minX(), minY(), minZ(), maxX(), maxY(), maxZ()};
    }

    [[nodiscard]] double minX() const { return position.x; }
    [[nodiscard]] double minY() const { return position.y; }
    [[nodiscard]] double minZ() const { return position.z; }
    [[nodiscard]] double maxX() const { return position.x + 1; }
    [[nodiscard]] double maxY() const { return position.y + 1; }
    [[nodiscard]] double maxZ() const { return position.z + 1; }

    [[nodiscard]] double calculateXOffset(const AABB &other, double dx) const;
    [[nodiscard]] double calculateYOffset(const AABB &other, double dy) const;
    [[nodiscard]] double calculateZOffset(const AABB &other, double dz) const;

    [[nodiscard]] virtual float slipperiness() const {
        return 0.6f;
    }

    [[nodiscard]] virtual std::string getName() const = 0;
    [[nodiscard]] virtual std::string getTextureName(int face) const = 0;

    virtual void onPlace() {}
    virtual void onBreak() {}

};

#endif
