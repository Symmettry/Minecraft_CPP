#ifndef MINECRAFTCLIENT_AABB_HPP
#define MINECRAFTCLIENT_AABB_HPP

struct AABB {
    float minX, minY, minZ;
    float maxX, maxY, maxZ;

    [[nodiscard]] AABB offset(const float x, const float y, const float z) const {
        return {minX + x, minY + y, minZ + z, maxX + x, maxY + y, maxZ + z};
    }

    [[nodiscard]] AABB expand(const float x, const float y, const float z) const {
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

    [[nodiscard]] float calcWidth() const { return maxX - minX; }
    [[nodiscard]] float calcHeight() const { return maxY - minY; }
    [[nodiscard]] float calcDepth() const { return maxZ - minZ; }
};

#endif
