#ifndef MINECRAFTCLIENT_AABB_HPP
#define MINECRAFTCLIENT_AABB_HPP

#include <glm/glm.hpp>

struct AABB {

    AABB() = default;
    AABB(const double minX, const double minY, const double minZ, const double maxX, const double maxY, const double maxZ) : minX(minX), minY(minY), minZ(minZ), maxX(maxX), maxY(maxY), maxZ(maxZ) {}
    AABB(const glm::vec3 min, const glm::vec3 max) : minX(min.x), minY(min.y), minZ(min.z), maxX(max.x), maxY(max.y), maxZ(max.z) {}

    double minX{}, minY{}, minZ{};
    double maxX{}, maxY{}, maxZ{};

    [[nodiscard]] AABB offset(const double x, const double y, const double z) const {
        return {minX + x, minY + y, minZ + z, maxX + x, maxY + y, maxZ + z};
    }

    [[nodiscard]] AABB expand(const double x, const double y, const double z) const {
        return {
            x < 0 ? minX + x : minX,
            y < 0 ? minY + y : minY,
            z < 0 ? minZ + z : minZ,
            x > 0 ? maxX + x : maxX,
            y > 0 ? maxY + y : maxY,
            z > 0 ? maxZ + z : maxZ
        };
    }

    [[nodiscard]] bool intersects(const AABB &other) const {
        return maxX > other.minX && minX < other.maxX &&
               maxY > other.minY && minY < other.maxY &&
               maxZ > other.minZ && minZ < other.maxZ;
    }

    [[nodiscard]] double calcWidth() const { return maxX - minX; }
    [[nodiscard]] double calcHeight() const { return maxY - minY; }
    [[nodiscard]] double calcDepth() const { return maxZ - minZ; }

    glm::vec3 min() const {
        return {minX, minY, minZ};
    }
};

#endif
