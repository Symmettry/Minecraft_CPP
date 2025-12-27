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

    void setBlockAt(const int worldX, const int worldY, const int worldZ, const BlockType type) {
        if (worldY < 0 || worldY >= CHUNK_SIZE) return;

        const int chunkX = (worldX >= 0 ? worldX / CHUNK_SIZE : (worldX - CHUNK_SIZE + 1) / CHUNK_SIZE);
        const int chunkZ = (worldZ >= 0 ? worldZ / CHUNK_SIZE : (worldZ - CHUNK_SIZE + 1) / CHUNK_SIZE);

        const int localX = (worldX % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        const int localZ = (worldZ % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;

        ChunkCoord coord{chunkX, chunkZ};
        if (!chunks.contains(coord)) {
            chunks.emplace(coord, Chunk(chunkX, chunkZ));
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

    std::vector<Block> getCollidingBlocks(const AABB &box) const;

    Block* getBlockAt(const int worldX, const int worldY, const int worldZ) {
        if (worldY < 0 || worldY >= CHUNK_SIZE) return nullptr;

        const int chunkX = (worldX >= 0 ? worldX / CHUNK_SIZE : (worldX - CHUNK_SIZE + 1) / CHUNK_SIZE);
        const int chunkZ = (worldZ >= 0 ? worldZ / CHUNK_SIZE : (worldZ - CHUNK_SIZE + 1) / CHUNK_SIZE);

        const int localX = (worldX % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        const int localZ = (worldZ % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;

        ChunkCoord coord{chunkX, chunkZ};
        if (!chunks.contains(coord)) {
            chunks.emplace(coord, Chunk(chunkX, chunkZ));
        }

        Chunk& chunk = chunks.at(coord);
        return chunk.getBlock(localX, worldY, localZ);
    }
};
