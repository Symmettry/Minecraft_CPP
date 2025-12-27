#include "Block.hpp"

[[nodiscard]] float Block::calculateXOffset(const AABB &other, float dx) const {
    if (!isOpaque()) return dx;

    if (const auto &[minX, minY, minZ, maxX, maxY, maxZ] = getBoundingBox();
        other.maxY > minY && other.minY < maxY &&
        other.maxZ > minZ && other.minZ < maxZ) {

        if (dx > 0.0f && other.maxX <= minX)
            dx = std::min(dx, minX - other.maxX);
        if (dx < 0.0f && other.minX >= maxX)
            dx = std::max(dx, maxX - other.minX);
    }

    return dx;
}

[[nodiscard]] float Block::calculateYOffset(const AABB &other, float dy) const {
    if (!isOpaque()) return dy;

    if (const auto &[minX, minY, minZ, maxX, maxY, maxZ] = getBoundingBox();
        other.maxX > minX && other.minX < maxX &&
        other.maxZ > minZ && other.minZ < maxZ) {

        if (dy > 0.0f && other.maxY <= minY)
            dy = std::min(dy, minY - other.maxY);
        if (dy < 0.0f && other.minY >= maxY)
            dy = std::max(dy, maxY - other.minY);
    }

    return dy;
}

[[nodiscard]] float Block::calculateZOffset(const AABB &other, float dz) const {
    if (!isOpaque()) return dz;

    if (const auto &[minX, minY, minZ, maxX, maxY, maxZ] = getBoundingBox();
        other.maxX > minX && other.minX < maxX &&
        other.maxY > minY && other.minY < maxY) {

        if (dz > 0.0f && other.maxZ <= minZ)
            dz = std::min(dz, minZ - other.maxZ);
        if (dz < 0.0f && other.minZ >= maxZ)
            dz = std::max(dz, maxZ - other.minZ);
    }

    return dz;
}