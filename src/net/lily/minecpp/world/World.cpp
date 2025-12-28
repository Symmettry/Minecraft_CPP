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

    const int minX = INT(std::floor(box.minX));
    const int maxX = INT(std::floor(box.maxX));
    const int minY = INT(std::floor(box.minY));
    const int maxY = INT(std::floor(box.maxY));
    const int minZ = INT(std::floor(box.minZ));
    const int maxZ = INT(std::floor(box.maxZ));

    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            for (int z = minZ; z <= maxZ; ++z) {
                if (y < 0 || y >= CHUNK_SIZE) continue;

                const int chunkX = (x >= 0 ? x / CHUNK_SIZE : (x - CHUNK_SIZE + 1) / CHUNK_SIZE);
                const int chunkZ = (z >= 0 ? z / CHUNK_SIZE : (z - CHUNK_SIZE + 1) / CHUNK_SIZE);
                const int localX = (x % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
                const int localZ = (z % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;

                ChunkCoord coord{chunkX, chunkZ};
                if (!chunks.contains(coord)) continue;
                const Chunk& chunk = chunks.at(coord);

                const std::unique_ptr<Block>& type = chunk.getBlock(localX, y, localZ);
                if (!type->isOpaque()) continue;

                collisions.push_back(type.get());
            }
        }
    }

    return collisions;
}
