#pragma once

#include <array>
#include <vector>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <atomic>
#include <cstring>
#include <mutex>
#include <glm/gtx/hash.hpp>
#include <glad/glad.hpp>

#include "./Block.hpp"
#include "net/lily/minecpp/render/BlockAtlas.hpp"
#include "net/lily/minecpp/render/Shader.hpp"

class World;
class BlockAtlas;

constexpr int CHUNK_SIZE = 16;
constexpr int HALF_CHUNK_SIZE = CHUNK_SIZE / 2;
constexpr int WORLD_HEIGHT = 256;

struct Vertex {
    uint32_t packed;
    uint32_t tint;
    uint32_t shade;
    uint32_t modelOffset;
    uint32_t modelUv;
};

struct MeshData {
    mutable std::vector<Vertex> vertices;
    mutable std::vector<Vertex> translucentVertices;
    mutable std::array<GLsizei, WORLD_HEIGHT / CHUNK_SIZE> sectionVertexCounts{};
    mutable std::array<GLsizei, WORLD_HEIGHT / CHUNK_SIZE> translucentSectionVertexCounts{};
    mutable std::vector<glm::ivec3> skullBlocks;
};

using PendingFace = uint64_t;

struct ivec3Hash {
    std::size_t operator()(const glm::ivec3& v) const noexcept {
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1) ^ (std::hash<int>()(v.z) << 2);
    }
};

struct Minecraft;
struct Plane;

class Chunk : public std::enable_shared_from_this<Chunk> {
public:
    const World* world;
    const Minecraft* mc;

    static std::atomic<int> loadingMeshes;

    mutable bool culled = false;
    mutable uint16_t visibleSections = 0xFFFF;

    static constexpr unsigned int TRIANGLES[2][3] = {
        {0,1,2}, {2,3,0}
    };
    static constexpr uint8_t OVERFLOW_X = 1 << 0, OVERFLOW_Y = 1 << 1, OVERFLOW_Z = 1 << 2;

    Chunk(int x, int z, const World* world);
    ~Chunk();

    void clearSection(const int sy) {
        constexpr int SECTION_VOLUME = 16 * 16 * 16;
        const int offset = sy * SECTION_VOLUME;
        std::memset(blocks.data() + offset, 0, SECTION_VOLUME * sizeof(uint16_t));
        std::memset(light.data() + offset, 0, SECTION_VOLUME);
        sectionPresent[sy] = false;
    }

    void setBiomes(const uint8_t* source) { std::memcpy(biomes.data(), source, biomes.size()); }
    void setSectionLight(const int sectionY, const uint8_t* blockSource, const uint8_t* skySource) {
        constexpr size_t sectionVolume = 16 * 16 * 16;
        const size_t offset = static_cast<size_t>(sectionY) * sectionVolume;
        for (size_t index = 0; index < sectionVolume; ++index) {
            light[offset + index] = static_cast<uint8_t>(
                (blockSource[index] & 15U) | ((skySource[index] & 15U) << 4U));
        }
        sectionPresent[sectionY] = true;
    }

    void testCull(const std::array<Plane, 6> &frustumPlanes, glm::vec3 cameraPos) const;

    mutable std::unordered_map<glm::ivec3, std::vector<PendingFace>, ivec3Hash> pendingFaces;
    mutable std::mutex meshMutex;
    mutable std::vector<glm::ivec3> uploadedSkullBlocks;

    void setBlock(int x, int y, int z, Block block);
    void relightSkylight();
    void relightBlockLight();
    // relights and returns the mask of 16-block sections whose faces' lighting may have changed
    uint32_t relightSkylightSections();
    static constexpr uint32_t ALL_SECTIONS = (1U << (WORLD_HEIGHT / CHUNK_SIZE)) - 1U;
    void initializeEmptySectionSkyLight(bool hasSky);

    [[nodiscard]] Block getBlock(int x, int y, int z) const;
    [[nodiscard]] uint8_t getBiome(const int x, const int z) const {
        return biomes[z * CHUNK_SIZE + x];
    }
    [[nodiscard]] uint8_t isOpaque(int x, int y, int z) const;
    [[nodiscard]] uint8_t occludesAmbientLight(int x, int y, int z) const;
    [[nodiscard]] uint32_t getLightData(int x, int y, int z) const;

    static uint16_t getTileIndex(const BlockAtlas &atlas, Block block, int face);

    void generateMesh(const BlockAtlasData &blockAtlas, uint32_t sectionMask = ALL_SECTIONS) const;
    void uploadMesh() const;

    void draw(GLint chunkOffsetLocation, const glm::vec3& chunkOffset) const;
    void drawTranslucent(GLint chunkOffsetLocation, const glm::vec3& chunkOffset) const;
    void queueMesh(const BlockAtlasData &blockAtlas, bool priority = false,
                   uint32_t sectionMask = ALL_SECTIONS) const;

    [[nodiscard]] double euclDistSqr(const glm::vec<3, double> &vec) const {
        const double dx = chunkX * CHUNK_SIZE + HALF_CHUNK_SIZE - vec.x;
        const double dz = chunkZ * CHUNK_SIZE + HALF_CHUNK_SIZE - vec.z;
        return dx * dx + dz * dz;
    }
    [[nodiscard]] double manhatDist(const glm::vec<3, double> &vec) const {
        const double dx = chunkX * CHUNK_SIZE + HALF_CHUNK_SIZE - vec.x;
        const double dz = chunkZ * CHUNK_SIZE + HALF_CHUNK_SIZE - vec.z;
        return std::abs(dx) + std::abs(dz);
    }

    const int chunkX, chunkZ;
    const glm::vec3 worldPos;

    mutable std::atomic<bool> loaded = false;
    mutable std::atomic_bool dataReady = false;
    mutable std::atomic_bool meshQueued = false;
    mutable std::atomic_bool meshDirty = false;

private:
    std::array<Block, CHUNK_SIZE * CHUNK_SIZE * WORLD_HEIGHT> blocks{};
    std::array<uint8_t, CHUNK_SIZE * CHUNK_SIZE> biomes{};
    std::array<uint8_t, CHUNK_SIZE * CHUNK_SIZE * WORLD_HEIGHT> light{};
    std::array<bool, WORLD_HEIGHT / 16> sectionPresent{};
    mutable MeshData meshData;
    // per-section mesh caches (guarded by meshMutex); only dirty sections are rebuilt
    mutable std::array<std::vector<Vertex>, WORLD_HEIGHT / CHUNK_SIZE> sectionOpaque, sectionTranslucent;
    mutable std::array<std::vector<glm::ivec3>, WORLD_HEIGHT / CHUNK_SIZE> sectionSkulls;
    mutable std::atomic<uint32_t> dirtySections{(1U << (WORLD_HEIGHT / CHUNK_SIZE)) - 1U};

    mutable GLsizeiptr vboCapacity = 0;
    mutable GLuint VAO = 0, VBO = 0;
    mutable GLsizei uploadedVertexCount = 0;
    mutable GLsizei uploadedTranslucentVertexCount = 0;
    mutable std::array<GLsizei, WORLD_HEIGHT / CHUNK_SIZE> uploadedSectionVertexCounts{};
    mutable std::array<GLsizei, WORLD_HEIGHT / CHUNK_SIZE> uploadedTranslucentSectionVertexCounts{};
    static constexpr int index(const int x, const int y, const int z) {
        return x + CHUNK_SIZE * (z + CHUNK_SIZE * y);
    }
};

struct MeshUploadJob {
    std::shared_ptr<const Chunk> chunk;
};

struct MeshDeleteJob {
    const GLuint vao;
    const GLuint vbo;
};
