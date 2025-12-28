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

struct EnumDifficulty {
    enum Value {
        PEACEFUL,
        EASY,
        NORMAL,
        HARD,
    } value;

    static Value fromId(const int id) {
        switch (id) {
            case 0: return PEACEFUL;
            case 1: return EASY;
            case 2: return NORMAL;
            case 3: return HARD;
            default: throw std::runtime_error("Invalid ID");
        }
    }
};
struct GameType {
    enum Value {
        NOT_SET,
        SURVIVAL,
        CREATIVE,
        ADVENTURE,
        SPECTATOR,
    } value;

    static Value fromID(const int id) {
        switch (id) {
            case 0: return NOT_SET;
            case 1: return SURVIVAL;
            case 2: return CREATIVE;
            case 3: return ADVENTURE;
            case 4: return SPECTATOR;
            default: throw std::runtime_error("Invalid ID");
        }
    }
};


class World {
public:
    mutable std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash> chunks;
    mutable std::vector<std::shared_ptr<Entity>> entities;

    mutable uint64_t totalTime, worldTime;

    void update() const;

    void setBlockAt(const int worldX, const int worldY, const int worldZ, const Material type) const {
        if (worldY < 0 || worldY >= WORLD_HEIGHT) return;

        const int chunkX = std::floor(static_cast<float>(worldX) / CHUNK_SIZE);
        const int chunkZ = std::floor(static_cast<float>(worldZ) / CHUNK_SIZE);

        const int localX = worldX - chunkX * CHUNK_SIZE;
        const int localZ = worldZ - chunkZ * CHUNK_SIZE;

       getOrMakeChunk(chunkX, chunkZ)->setBlock(localX, worldY, localZ, type);
    }

    template <typename Func>
    void forEachChunk(Func&& f) {
        for (auto &chunk: chunks | std::views::values) {
            f(chunk);
        }
    }

    std::vector<const Block *> getCollidingBlocks(const AABB &box) const;

    Chunk* getLoadedChunkAt(const int worldX, const int worldZ) const {
        const int chunkX = std::floor(static_cast<float>(worldX) / CHUNK_SIZE);
        const int chunkZ = std::floor(static_cast<float>(worldZ) / CHUNK_SIZE);
        const ChunkCoord coord{chunkX, chunkZ};
        const auto it = chunks.find(coord);
        if (it == chunks.end()) return nullptr;
        return &it->second;
    }
    Chunk* getOrMakeChunk(const int x, const int z) const {
        ChunkCoord coord{x, z};
        auto it = chunks.find(coord);
        if (it == chunks.end()) {
            it = chunks.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(coord),
                std::forward_as_tuple(x, z)
            ).first;
        }
        return &it->second;
    }


    const Block* getBlockAt(const glm::vec<3, int> vec) const {
        return getBlockAt(vec.x, vec.y, vec.z);
    }
    const Block* getBlockAt(const int worldX, const int worldY, const int worldZ) const {
        if (worldY < 0 || worldY >= WORLD_HEIGHT) return nullptr;

        int localX = worldX % CHUNK_SIZE;
        if (localX < 0) localX += CHUNK_SIZE;

        int localZ = worldZ % CHUNK_SIZE;
        if (localZ < 0) localZ += CHUNK_SIZE;

        const Chunk* chunk = getLoadedChunkAt(worldX, worldZ);
        if (!chunk) return new AirBlock(worldX, worldY, worldZ);

        const std::unique_ptr<Block>& blockPtr = chunk->getBlock(localX, worldY, localZ);
        return blockPtr.get();
    }

    [[nodiscard]] bool isChunkAtLoaded(const double x, const double z) const {
        const int chunkX = std::floor(static_cast<float>(x) / CHUNK_SIZE);
        const int chunkZ = std::floor(static_cast<float>(z) / CHUNK_SIZE);

        const ChunkCoord coord{chunkX, chunkZ};
        return chunks.contains(coord);
    }
};
