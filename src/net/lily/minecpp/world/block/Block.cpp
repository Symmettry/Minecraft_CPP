#include "Block.hpp"

[[nodiscard]] double Block::calculateXOffset(const AABB &other, double dx) const {
    if (!isOpaque()) return dx;

    if (const auto &[minX, minY, minZ, maxX, maxY, maxZ] = getBoundingBox();
        other.maxY > minY && other.minY < maxY &&
        other.maxZ > minZ && other.minZ < maxZ) {

        if (dx > 0.0 && other.maxX <= minX)
            dx = std::min(dx, minX - other.maxX);
        if (dx < 0.0 && other.minX >= maxX)
            dx = std::max(dx, maxX - other.minX);
    }

    return dx;
}

[[nodiscard]] double Block::calculateYOffset(const AABB &other, double dy) const {
    if (!isOpaque()) return dy;

    if (const auto &[minX, minY, minZ, maxX, maxY, maxZ] = getBoundingBox();
        other.maxX > minX && other.minX < maxX &&
        other.maxZ > minZ && other.minZ < maxZ) {

        if (dy > 0.0 && other.maxY <= minY)
            dy = std::min(dy, minY - other.maxY);
        if (dy < 0.0 && other.minY >= maxY)
            dy = std::max(dy, maxY - other.minY);
    }

    return dy;
}

[[nodiscard]] double Block::calculateZOffset(const AABB &other, double dz) const {
    if (!isOpaque()) return dz;

    if (const auto &[minX, minY, minZ, maxX, maxY, maxZ] = getBoundingBox();
        other.maxX > minX && other.minX < maxX &&
        other.maxY > minY && other.minY < maxY) {

        if (dz > 0.0 && other.maxZ <= minZ)
            dz = std::min(dz, minZ - other.maxZ);
        if (dz < 0.0 && other.minZ >= maxZ)
            dz = std::max(dz, maxZ - other.minZ);
    }

    return dz;
}