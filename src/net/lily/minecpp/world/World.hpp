#pragma once

#include <memory>
#include <ranges>
#include <unordered_map>
#include "Chunk.hpp"
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
    std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash> chunks;
    std::vector<std::shared_ptr<Entity>> entities;

    void update() const;

    void setBlockAt(const int worldX, const int worldY, const int worldZ, const Material type) {
        if (worldY < 0 || worldY >= CHUNK_SIZE) return;

        const int chunkX = (worldX >= 0 ? worldX / CHUNK_SIZE : (worldX - CHUNK_SIZE + 1) / CHUNK_SIZE);
        const int chunkZ = (worldZ >= 0 ? worldZ / CHUNK_SIZE : (worldZ - CHUNK_SIZE + 1) / CHUNK_SIZE);

        const int localX = (worldX % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        const int localZ = (worldZ % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;

        ChunkCoord coord{chunkX, chunkZ};
        if (!chunks.contains(coord)) {
            chunks.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(coord),
                std::forward_as_tuple(chunkX, chunkZ)
            );
        }

        Chunk& chunk = chunks.at(coord);
        chunk.setBlock(localX, worldY, localZ, type);
    }

    template <typename Func>
    void forEachChunk(Func&& f) {
        for (auto &chunk: chunks | std::views::values) {
            f(chunk);
        }
    }

    std::vector<const Block *> getCollidingBlocks(const AABB &box) const;

    const Block* getBlockAt(const glm::vec<3, int> vec) {
        return getBlockAt(vec.x, vec.y, vec.z);
    }
    const Block* getBlockAt(const int worldX, const int worldY, const int worldZ) {
        if (worldY < 0 || worldY >= CHUNK_SIZE) return nullptr;

        const int chunkX = (worldX >= 0 ? worldX / CHUNK_SIZE : (worldX - CHUNK_SIZE + 1) / CHUNK_SIZE);
        const int chunkZ = (worldZ >= 0 ? worldZ / CHUNK_SIZE : (worldZ - CHUNK_SIZE + 1) / CHUNK_SIZE);

        const int localX = (worldX % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        const int localZ = (worldZ % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;

        ChunkCoord coord{chunkX, chunkZ};

        auto it = chunks.find(coord);
        if (it == chunks.end()) {
            it = chunks.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(coord),
                std::forward_as_tuple(chunkX, chunkZ)
            ).first;
        }

        const Chunk& chunk = it->second;
        const std::unique_ptr<Block>& blockPtr = chunk.getBlock(localX, worldY, localZ);
        return blockPtr ? blockPtr.get() : nullptr;
    }
};
