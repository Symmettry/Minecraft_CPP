#pragma once

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <utility>
#include <ranges>
#include <unordered_map>
#include <cmath>
#include "chunk/Chunk.hpp"
#include "net/lily/minecpp/entity/Entity.hpp"
#include "blockentity/BlockEntity.hpp"

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
        NOT_SET = -1,
        SURVIVAL = 0,
        CREATIVE = 1,
        ADVENTURE = 2,
        SPECTATOR = 3,
    } value;

    static Value fromID(const int id) {
        switch (id) {
            case 0: return SURVIVAL;
            case 1: return CREATIVE;
            case 2: return ADVENTURE;
            case 3: return SPECTATOR;
            default: throw std::runtime_error("Invalid ID");
        }
    }
};


class World {
public:
    struct BlockActionEvent {
        glm::ivec3 position{};
        uint8_t data1 = 0;
        uint8_t data2 = 0;
        Block block = BLOCK_AIR;
    };

    struct DestroyBlockProgress {
        glm::ivec3 position{};
        uint8_t progress = 0;
    };

    struct PistonMovement {
        glm::ivec3 position{};
        glm::ivec3 direction{};
        uint8_t tick = 0;
    };

    const Minecraft* mc;

    mutable std::unordered_map<ChunkCoord, std::shared_ptr<Chunk>, ChunkCoordHash> chunks;
    mutable std::recursive_mutex chunksMutex;
    mutable uint64_t chunkRevision = 0;
    mutable std::vector<std::shared_ptr<Entity>> entities;
    mutable std::recursive_mutex entityMutex;
    mutable std::mutex blockEventMutex;
    mutable std::vector<BlockActionEvent> blockActions;
    mutable std::vector<PistonMovement> pistonMovements;
    mutable std::unordered_map<int32_t, DestroyBlockProgress> damagedBlocks;
    mutable std::unordered_map<uint64_t, BlockEntity> blockEntities;
    mutable std::shared_mutex blockEntityMutex;

    mutable std::unordered_map<ChunkCoord, std::vector<std::pair<glm::ivec3, PendingFace>>, ChunkCoordHash> pendingFace4Chunks;

    mutable std::atomic<int64_t> totalTime, worldTime;
    mutable std::atomic_bool daylightCycle = true;

    explicit World(const Minecraft* mc) : mc(mc), totalTime(0), worldTime(0) { }

    void update() const;
    void queuePistonMovement(const glm::ivec3& position, uint8_t facing) const;
    [[nodiscard]] std::shared_ptr<Entity> getEntityById(int entityId) const;
    void addEntity(const std::shared_ptr<Entity>& entity) const;
    void removeEntity(int entityId) const;
    // queues remeshes of only the sections a block change at this position can affect
    void queueBlockChangeMesh(int chunkX, int chunkZ, int localX, int worldY, int localZ,
                              uint32_t lightSections) const;
    static uint32_t sectionsAround(int worldY);
    [[nodiscard]] std::vector<std::shared_ptr<Entity>> snapshotEntities() const;
    [[nodiscard]] bool hasEntityCollision(const AABB& bounds) const;
    static uint64_t positionKey(const glm::ivec3& position) {
        return (static_cast<uint64_t>(position.x) & 0x3FFFFFFULL) << 38U
            | (static_cast<uint64_t>(position.y) & 0xFFFULL) << 26U
            | (static_cast<uint64_t>(position.z) & 0x3FFFFFFULL);
    }
    void setBlockEntity(const glm::ivec3& position, BlockEntity blockEntity) const {
        std::unique_lock lock(blockEntityMutex);
        blockEntities.insert_or_assign(positionKey(position), std::move(blockEntity));
    }
    [[nodiscard]] std::optional<BlockEntity> getBlockEntity(const glm::ivec3& position) const {
        std::shared_lock lock(blockEntityMutex);
        const auto found = blockEntities.find(positionKey(position));
        return found == blockEntities.end() ? std::nullopt : std::optional<BlockEntity>(found->second);
    }

    [[nodiscard]] float celestialAngle(const float partialTicks) const {
        const int64_t time = worldTime.load(std::memory_order_relaxed);
        float angle = static_cast<float>(std::fmod(static_cast<double>(time) + partialTicks, 24000.0) / 24000.0 - 0.25);
        if (angle < 0.0f) ++angle;
        if (angle > 1.0f) --angle;
        const float original = angle;
        angle = 1.0f - (std::cos(angle * static_cast<float>(M_PI)) + 1.0f) * 0.5f;
        return original + (angle - original) / 3.0f;
    }

    [[nodiscard]] float sunBrightness(const float partialTicks) const {
        const float angle = celestialAngle(partialTicks);
        float darkness = 1.0f - (std::cos(angle * static_cast<float>(M_PI) * 2.0f) * 2.0f + 0.2f);
        darkness = std::clamp(darkness, 0.0f, 1.0f);
        return (1.0f - darkness) * 0.8f + 0.2f;
    }

    [[nodiscard]] int skylightSubtracted(const float partialTicks) const {
        const float angle = celestialAngle(partialTicks);
        float darkness = 1.0f - (std::cos(angle * static_cast<float>(M_PI) * 2.0f) * 2.0f + 0.5f);
        darkness = std::clamp(darkness, 0.0f, 1.0f);
        return static_cast<int>(darkness * 11.0f);
    }

    [[nodiscard]] uint8_t getBiomeAt(const int worldX, const int worldZ) const {
        const std::shared_ptr<Chunk> chunk = getLoadedChunkAt(worldX, worldZ);
        return chunk == nullptr ? 1 : chunk->getBiome(toLocalX(worldX), toLocalZ(worldZ));
    }

    void addPendingFace(const int ncX, const int ncZ, const glm::ivec3 pos, const PendingFace faceData) const {
        std::lock_guard lock(chunksMutex);
        const ChunkCoord coord{ncX, ncZ};
        pendingFace4Chunks[coord].push_back({pos, faceData});
    }

    static const Block airBlock;

    void setBlockAt(int worldX, int worldY, int worldZ, Block value) const;

    template <typename Func>
    void forEachChunk(Func&& f) {
        for (const auto& [position, chunk] : snapshotChunks()) {
            f(chunk);
        }
    }

    [[nodiscard]] std::vector<std::pair<ChunkCoord, std::shared_ptr<Chunk>>> snapshotChunks() const {
        std::lock_guard lock(chunksMutex);
        std::vector<std::pair<ChunkCoord, std::shared_ptr<Chunk>>> snapshot;
        snapshot.reserve(chunks.size());
        for (const auto& entry : chunks) snapshot.push_back(entry);
        return snapshot;
    }

    bool refreshChunkSnapshot(uint64_t& revision,
                              std::vector<std::pair<ChunkCoord, std::shared_ptr<Chunk>>>& snapshot) const {
        std::lock_guard lock(chunksMutex);
        if (revision == chunkRevision) return false;
        snapshot.clear();
        snapshot.reserve(chunks.size());
        for (const auto& entry : chunks) snapshot.push_back(entry);
        revision = chunkRevision;
        return true;
    }

    void removeChunk(const int chunkX, const int chunkZ) const {
        std::lock_guard lock(chunksMutex);
        if (chunks.erase({chunkX, chunkZ}) != 0) ++chunkRevision;
    }

    std::vector<LocalBlock> getCollidingBlocks(const AABB &box) const;

    std::shared_ptr<Chunk> getLoadedChunk(const int chunkX, const int chunkZ) const {
        std::lock_guard lock(chunksMutex);
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
        std::lock_guard lock(chunksMutex);
        ChunkCoord coord{x, z};
        auto it = chunks.find(coord);
        if (it == chunks.end()) {
            const auto chunkPtr = std::make_shared<Chunk>(x, z, this);
            it = chunks.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(coord),
                std::forward_as_tuple(chunkPtr)
            ).first;
            ++chunkRevision;
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
        std::lock_guard lock(chunksMutex);
        const int chunkX = std::floor(x / CHUNK_SIZE);
        const int chunkZ = std::floor(z / CHUNK_SIZE);
        const ChunkCoord coord{chunkX, chunkZ};
        return chunks.contains(coord);
    }
};
