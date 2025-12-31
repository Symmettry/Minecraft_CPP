#pragma once
#include <cstdint>

#include "net/lily/minecpp/util/AABB.hpp"

#define F false
#define N nullptr
#define SC 0.6f

using Block = uint16_t;

constexpr Block BLOCK_AIR   = 0;
constexpr Block BLOCK_STONE = 1 << 4;
constexpr Block BLOCK_GRASS = 2 << 4;
constexpr Block BLOCK_DIRT  = 3 << 4;
constexpr Block BLOCK_WATER = 9 << 4;
constexpr Block BLOCK_ICE   = 79 << 4;
constexpr Block BLOCK_OBSIDIAN = 49 << 4;

constexpr uint16_t blockId(const Block b) {
    return b >> 4;
}

constexpr uint8_t blockMeta(const Block b) {
    return b & 0xF;
}

constexpr int BLOCK_COUNT = 256;

constexpr bool BLOCK_OPAQUE[BLOCK_COUNT] = {
    /* 0 Air */ F,
    /* 1 Stone */ true,
    /* 2 Grass */ true,
    /* 3 Dirt */ true,
    F,F,F,F,F,
    /* 9 Water */ F,
    F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,
    /* 49 Obsidian */ true,
    F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,
    /* 79 Ice */ F,
};

constexpr const char* SINGLE_FACE_SET[BLOCK_COUNT] = {
    N, /* 0 Air */
    "stone",
    "grass_side", /* 2 Grass */
    "dirt",
    N,N,N,N,N,
    "water_still",
    N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,
    "obsidian",
    N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,
    "ice",
};

constexpr const char* GRASS_FACE[] = { nullptr, nullptr, nullptr, nullptr, "grass_top", "dirt" };
constexpr const char* const* MULTI_FACE_SET[BLOCK_COUNT] = {
    N,      // 0 Air
    N,      // 1 Stone
    GRASS_FACE,  // 2 Grass
    N,      // 3 Dirt
    N, N, N, N, N,
    N,      // 9 Water
    N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,
    N,      // 49 Obsidian
    N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,
    N,      // 79 Ice
};

// thank you c++ for false = 0 <3
constexpr float BLOCK_SLIPPERINESS[BLOCK_COUNT] = {
    /* 0 Air */ SC,
    /* 1 Stone */ SC,
    /* 2 Grass */ SC,
    /* 3 Dirt */ SC,
    SC,SC,SC,SC,SC,
    /* 9 Water */ SC,
    SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,
    /* Obsidian */ SC,
    SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,SC,
    /* Ice */ 0.98f,
};

// TODO: Replace with assets/minecraft/models/block/xxx.json
constexpr const char* getBlockTexture(const int id, const int face) {
    return MULTI_FACE_SET[id] && MULTI_FACE_SET[id][face] ? MULTI_FACE_SET[id][face] : SINGLE_FACE_SET[id];
}

constexpr bool isOpaque(const Block block) {
    return BLOCK_OPAQUE[blockId(block)];
}

constexpr float getSlipperiness(const Block block) {
    return BLOCK_SLIPPERINESS[blockId(block)];
}

struct LocalBlock {
    const Block block;
    const int x, y, z;

    [[nodiscard]] double calculateXOffset(const AABB &other, double dx) const {
        if (!isOpaque(block)) return dx;

        const double minX = x, minY = y, minZ = z,
                     maxX = x + 1, maxY = y + 1, maxZ = z + 1;

        if (other.maxY > minY && other.minY < maxY &&
            other.maxZ > minZ && other.minZ < maxZ) {

            if (dx > 0.0 && other.maxX <= minX)
                dx = std::min(dx, minX - other.maxX);
            if (dx < 0.0 && other.minX >= maxX)
                dx = std::max(dx, maxX - other.minX);
            }

        return dx;
    }

    [[nodiscard]] double calculateYOffset(const AABB &other, double dy) const {
        if (!isOpaque(block)) return dy;

        const double minX = x, minY = y, minZ = z,
                     maxX = x + 1, maxY = y + 1, maxZ = z + 1;

        if (other.maxX > minX && other.minX < maxX &&
            other.maxZ > minZ && other.minZ < maxZ) {

            if (dy > 0.0 && other.maxY <= minY)
                dy = std::min(dy, minY - other.maxY);
            if (dy < 0.0 && other.minY >= maxY)
                dy = std::max(dy, maxY - other.minY);
            }

        return dy;
    }

    [[nodiscard]] double calculateZOffset(const AABB &other, double dz) const {
        if (!isOpaque(block)) return dz;

        const double minX = x, minY = y, minZ = z,
                     maxX = x + 1, maxY = y + 1, maxZ = z + 1;

        if (other.maxX > minX && other.minX < maxX &&
            other.maxY > minY && other.minY < maxY) {

            if (dz > 0.0 && other.maxZ <= minZ)
                dz = std::min(dz, minZ - other.maxZ);
            if (dz < 0.0 && other.minZ >= maxZ)
                dz = std::max(dz, maxZ - other.minZ);
            }

        return dz;
    }
};
