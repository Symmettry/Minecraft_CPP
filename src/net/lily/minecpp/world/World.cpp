#define INT static_cast<int>
#define FLOAT static_cast<float>

#include "World.hpp"
#include "chunk/Block.hpp"

void World::update() const {
    for (const auto& entity : entities) {
        entity->update();
    }
}

const Block World::airBlock = BLOCK_AIR;

std::vector<LocalBlock> World::getCollidingBlocks(const AABB &box) const {
    std::vector<LocalBlock> collisions;

    for (int x = floor(box.minX); x <= box.maxX; ++x) {
        for (int y = floor(box.minY); y <= box.maxY; ++y) {
            if (y < 0 || y >= WORLD_HEIGHT) continue;
            for (int z = floor(box.minZ); z <= box.maxZ; ++z) {
                const Block block = getBlockAt(x, y, z);
                if (!block || !BlockUtil::isOpaque(block)) continue;
                collisions.push_back({block, x, y, z});
            }
        }
    }

    return collisions;
}
