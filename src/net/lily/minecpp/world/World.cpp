#define INT static_cast<int>
#define FLOAT static_cast<float>

#include "World.hpp"

void World::update() const {
    for (const auto& entity : entities) {
        entity->update();
    }
}

std::vector<const Block *> World::getCollidingBlocks(const AABB &box) const {
    std::vector<const Block*> collisions;

    const int minX = static_cast<int>(std::floor(box.minX));
    const int maxX = static_cast<int>(std::ceil(box.maxX)) - 1;
    const int minY = static_cast<int>(std::floor(box.minY));
    const int maxY = static_cast<int>(std::ceil(box.maxY)) - 1;
    const int minZ = static_cast<int>(std::floor(box.minZ));
    const int maxZ = static_cast<int>(std::ceil(box.maxZ)) - 1;

    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            if (y < 0 || y >= WORLD_HEIGHT) continue;
            for (int z = minZ; z <= maxZ; ++z) {
                const Block* block = getBlockAt(x, y, z);
                if (!block || !block->isOpaque()) continue;
                collisions.push_back(block);
            }
        }
    }

    return collisions;
}
