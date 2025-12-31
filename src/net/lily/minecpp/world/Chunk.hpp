#pragma once

#include "block/Block.hpp"
#include <vector>
#include <array>
#include <memory>
#include <glm/glm.hpp>
#include <glad/glad.h>

class BlockAtlas;
constexpr int CHUNK_SIZE = 16;
constexpr int WORLD_HEIGHT = 256;

struct MeshData {
    mutable std::vector<float> vertices;
    mutable std::vector<unsigned int> indices;
};

class Chunk {
public:
    Chunk(int x, int z);
    ~Chunk();

    void setBlock(int x, int y, int z, Material type, bool unsafe = false);

    [[nodiscard]] const std::unique_ptr<Block> &getBlock(int x, int y, int z) const;

    [[nodiscard]] bool isOpaque(int nx, int ny, int nz) const;

    void generateMesh(const BlockAtlas* blockAtlas) const;

    static std::tuple<float, float, float, float> getTextureUV(const BlockAtlas *blockAtlas, const Block *block, int face);

    void uploadMesh() const; // send data to GPU

    void draw() const;

    void queueMesh() const;

    int chunkX, chunkZ;

private:
    std::array<std::unique_ptr<Block>, CHUNK_SIZE*WORLD_HEIGHT*CHUNK_SIZE> blocks;
    MeshData meshData;

    mutable unsigned int VAO = 0, VBO = 0, EBO = 0;

    static int index(const int x, const int y, const int z) {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y > WORLD_HEIGHT || z < 0 || z >= CHUNK_SIZE)
            throw std::out_of_range("Index called out of range");
        return x + CHUNK_SIZE * (z + CHUNK_SIZE * y);
    }
};

struct MeshUploadJob {
    const Chunk* chunk;
};