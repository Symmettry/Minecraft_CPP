#pragma once

#include <array>
#include <vector>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glad/glad.h>

#include "./Block.hpp"
#include "net/lily/minecpp/render/BlockAtlas.hpp"
#include "net/lily/minecpp/render/Shader.hpp"

class World;
class BlockAtlas;

constexpr int CHUNK_SIZE = 16;
constexpr int HALF_CHUNK_SIZE = CHUNK_SIZE / 2;
constexpr int WORLD_HEIGHT = 256;

struct Vertex {
    float x;        // 4B
    float y;        // 4B
    float z;        // 4B
    uint16_t tile;  // 2B
    uint8_t corner; // 1B
};

struct MeshData {
    mutable std::vector<Vertex> vertices;
    mutable std::vector<unsigned int> indices;
};

using PendingFace = uint16_t;

struct ivec3Hash {
    std::size_t operator()(const glm::ivec3& v) const noexcept {
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1) ^ (std::hash<int>()(v.z) << 2);
    }
};

class Chunk : public std::enable_shared_from_this<Chunk> {
public:
    const World* world;

    Chunk(int x, int z, const World* world);
    ~Chunk();

    mutable std::unordered_map<glm::ivec3, std::vector<PendingFace>, ivec3Hash> pendingFaces;

    int getMHChunkDist(const std::shared_ptr<Chunk> & chunk) const {
        if (!chunk) return 0;
        return std::abs(chunkX - chunk->chunkX) + std::abs(chunkZ - chunk->chunkZ);
    }

    static Shader* boundShader;

    void setBlock(int x, int y, int z, Block block);

    [[nodiscard]] Block getBlock(int x, int y, int z) const;
    [[nodiscard]] uint8_t isOpaque(int x, int y, int z) const;

    void generateMesh(const BlockAtlasData &blockAtlas) const;
    void uploadMesh() const;

    void initBoundary() const;

    void drawBoundaries(const glm::mat4 &projView) const;

    void draw(const Shader* blockShader, glm::mat4& model) const;
    void queueMesh(const BlockAtlasData &blockAtlas) const;

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

    mutable bool loaded = false;

private:
    std::array<Block, CHUNK_SIZE * CHUNK_SIZE * WORLD_HEIGHT> blocks{};
    mutable MeshData meshData;

    mutable GLuint VAO = 0, VBO = 0, EBO = 0;
    mutable GLuint boundaryVAO = 0, boundaryVBO = 0;

    mutable int boundaryVertexCount{};

    static constexpr int index(const int x, const int y, const int z) {
        return x + CHUNK_SIZE * (z + CHUNK_SIZE * y);
    }
};

struct MeshUploadJob {
    const std::shared_ptr<const Chunk> chunk;
};
