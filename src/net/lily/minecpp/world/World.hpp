#pragma once

#include <memory>
#include <ranges>
#include <unordered_map>
#include "chunk/Chunk.hpp"
#include "net/lily/minecpp/entity/Entity.hpp"

struct ChunkCoord {
    const int x, z;
    constexpr bool operator==(const ChunkCoord& other) const { return x == other.x && z == other.z; }
};

struct ChunkCoordHash {
    std::size_t operator()(const ChunkCoord& c) const {
        const std::size_t h1 = std::hash<int>()(c.x);
        const std::size_t h2 = std::hash<int>()(c.z);
        return h1 * 31 + h2;
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
    mutable std::unordered_map<ChunkCoord, std::shared_ptr<Chunk>, ChunkCoordHash> chunks;
    mutable std::vector<std::shared_ptr<Entity>> entities;

    mutable std::unordered_map<ChunkCoord, std::vector<std::pair<glm::ivec3, uint16_t>>, ChunkCoordHash> pendingFace4Chunks;

    mutable uint64_t totalTime, worldTime;

    void update() const;

    void addPendingFace(const int ncX, const int ncZ, const glm::ivec3 pos, const uint16_t faceData) const {
        const ChunkCoord coord{ncX, ncZ};
        pendingFace4Chunks[coord].push_back({pos, faceData});
    }

    static const Block airBlock;

    void setBlockAt(const int worldX, const int worldY, const int worldZ, const Block value) const {
        if (worldY < 0 || worldY >= WORLD_HEIGHT) return;

        const int chunkX = std::floor(static_cast<float>(worldX) / CHUNK_SIZE);
        const int chunkZ = std::floor(static_cast<float>(worldZ) / CHUNK_SIZE);

        const int localX = worldX - chunkX * CHUNK_SIZE;
        const int localZ = worldZ - chunkZ * CHUNK_SIZE;

       getOrMakeChunk(chunkX, chunkZ)->setBlock(localX, worldY, localZ, value);
    }

    template <typename Func>
    void forEachChunk(Func&& f) {
        for (auto &chunk: chunks | std::views::values) {
            f(chunk);
        }
    }

    std::vector<LocalBlock> getCollidingBlocks(const AABB &box) const;

    std::shared_ptr<Chunk> getLoadedChunk(const int chunkX, const int chunkZ) const {
        const ChunkCoord coord{chunkX, chunkZ};
        const auto it = chunks.find(coord);
        if (it == chunks.end()) return nullptr;
        return it->second;
    }
    std::shared_ptr<Chunk> getLoadedChunkAt(const int worldX, const int worldZ) const {
        const int chunkX = std::floor(static_cast<float>(worldX) / CHUNK_SIZE);
        const int chunkZ = std::floor(static_cast<float>(worldZ) / CHUNK_SIZE);
        return getLoadedChunk(chunkX, chunkZ);
    }

    std::shared_ptr<Chunk> getOrMakeChunk(const int x, const int z) const {
        ChunkCoord coord{x, z};
        auto it = chunks.find(coord);
        if (it == chunks.end()) {
            const auto chunkPtr = std::make_shared<Chunk>(x, z, this);
            it = chunks.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(coord),
                std::forward_as_tuple(chunkPtr)
            ).first;
        }
        return it->second;
    }

    static constexpr int toLocalX(const int worldX) {
        int localX = worldX % CHUNK_SIZE;
        if (localX < 0) localX += CHUNK_SIZE;
        return localX;
    }
    static constexpr int toLocalZ(const int worldZ) {
        int localZ = worldZ % CHUNK_SIZE;
        if (localZ < 0) localZ += CHUNK_SIZE;
        return localZ;
    }

    Block getBlockAt(const glm::vec<3, int> vec) const {
        return getBlockAt(vec.x, vec.y, vec.z);
    }
    Block getBlockAt(const int worldX, const int worldY, const int worldZ) const {
        if (worldY < 0 || worldY >= WORLD_HEIGHT) return airBlock;

        const std::shared_ptr<Chunk> chunk = getLoadedChunkAt(worldX, worldZ);
        if (!chunk) return BLOCK_AIR;

        return chunk->getBlock(toLocalX(worldX), worldY, toLocalZ(worldZ));
    }

    uint8_t isOpaque(const int worldX, const int worldY, const int worldZ) const {
        if (worldY < 0 || worldY >= WORLD_HEIGHT) return 0;

        const std::shared_ptr<Chunk> chunk = getLoadedChunkAt(worldX, worldZ);
        if (!chunk) return 2;
        return chunk->isOpaque(toLocalX(worldX), worldY, toLocalZ(worldZ));
    }

    [[nodiscard]] bool isChunkAtLoaded(const double x, const double z) const {
        const int chunkX = std::floor(x / CHUNK_SIZE);
        const int chunkZ = std::floor(z / CHUNK_SIZE);
        const ChunkCoord coord{chunkX, chunkZ};
        return chunks.contains(coord);
    }
};
