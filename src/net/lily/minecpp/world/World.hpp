#pragma once

#include <memory>
#include <ranges>
#include <unordered_map>
#include "Chunk.hpp"
#include "block/impl/AirBlock.hpp"
#include "net/lily/minecpp/entity/Entity.hpp"

struct ChunkCoord {
    int x, z;
    bool operator==(const ChunkCoord& other) const { return x == other.x && z == other.z; }
};

struct ChunkCoordHash {
    std::size_t operator()(const ChunkCoord& c) const {
        return std::hash<int>()(c.x) ^ (std::hash<int>()(c.z) << 1);
    }
};

class World {
public:
    mutable std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash> chunks;
    mutable std::vector<std::shared_ptr<Entity>> entities;

    void update() const;

    void setBlockAt(const int worldX, const int worldY, const int worldZ, const Material type) const {
        if (worldY < 0 || worldY >= WORLD_HEIGHT) return;

        const int chunkX = std::floor(static_cast<float>(worldX) / CHUNK_SIZE);
        const int chunkZ = std::floor(static_cast<float>(worldZ) / CHUNK_SIZE);

        const int localX = worldX - chunkX * CHUNK_SIZE;
        const int localZ = worldZ - chunkZ * CHUNK_SIZE;

        ChunkCoord coord{chunkX, chunkZ};
        auto it = chunks.find(coord);
        if (it == chunks.end()) {
            it = chunks.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(coord),
                std::forward_as_tuple(chunkX, chunkZ)
            ).first;
        }

        Chunk& chunk = it->second;
        chunk.setBlock(localX, worldY, localZ, type);
    }

    template <typename Func>
    void forEachChunk(Func&& f) {
        for (auto &chunk: chunks | std::views::values) {
            f(chunk);
        }
    }

    std::vector<const Block *> getCollidingBlocks(const AABB &box) const;

    const Block* getBlockAt(const glm::vec<3, int> vec) const {
        return getBlockAt(vec.x, vec.y, vec.z);
    }
    const Block* getBlockAt(const int worldX, const int worldY, const int worldZ) const {
        if (worldY < 0 || worldY >= WORLD_HEIGHT) return nullptr;

        int chunkX = std::floor(static_cast<float>(worldX) / CHUNK_SIZE);
        int chunkZ = std::floor(static_cast<float>(worldZ) / CHUNK_SIZE);

        int localX = worldX % CHUNK_SIZE;
        if (localX < 0) localX += CHUNK_SIZE;

        int localZ = worldZ % CHUNK_SIZE;
        if (localZ < 0) localZ += CHUNK_SIZE;

        ChunkCoord coord{chunkX, chunkZ};
        auto it = chunks.find(coord);
        if (it == chunks.end()) return new AirBlock(worldX, worldY, worldZ);

        const Chunk& chunk = it->second;
        const std::unique_ptr<Block>& blockPtr = chunk.getBlock(localX, worldY, localZ);
        return blockPtr.get();
    }
};
