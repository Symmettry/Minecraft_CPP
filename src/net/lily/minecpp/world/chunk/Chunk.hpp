#pragma once

#include <array>
#include <vector>
#include <tuple>
#include <glm/glm.hpp>
#include <glad/glad.h>

#include "./Block.hpp"
#include "net/lily/minecpp/render/BlockAtlas.hpp"
#include "net/lily/minecpp/render/Camera.hpp"
#include "net/lily/minecpp/render/Shader.hpp"

class BlockAtlas;

constexpr int CHUNK_SIZE = 16;
constexpr int HALF_CHUNK_SIZE = CHUNK_SIZE / 2;
constexpr int WORLD_HEIGHT = 256;

struct MeshData {
    mutable std::vector<float> vertices;
    mutable std::vector<unsigned int> indices;
};

class Chunk {
public:
    Chunk(int x, int z);
    ~Chunk();

    int getMHChunkDist(const std::shared_ptr<Chunk> & chunk) const {
        if (!chunk) return 0;
        return std::abs(chunkX - chunk->chunkX) + std::abs(chunkZ - chunk->chunkZ);
    }

    static Shader* boundShader;

    void setBlock(int x, int y, int z, Block block);

    [[nodiscard]] Block getBlock(int x, int y, int z) const;
    [[nodiscard]] bool isOpaque(int x, int y, int z) const;

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
    const Chunk* chunk;
};
