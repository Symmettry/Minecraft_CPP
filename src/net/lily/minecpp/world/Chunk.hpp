#pragma once

#include "block/Block.hpp"
#include <vector>
#include <array>
#include <glm/glm.hpp>
#include <glad/glad.h>

constexpr int CHUNK_SIZE = 16;
constexpr int WORLD_HEIGHT = 256;

struct MeshData {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
};

class Chunk {
public:
    Chunk(int x, int z);
    ~Chunk();

    void setBlock(int x, int y, int z, BlockType type);

    Block *getBlock(int x, int y, int z);
    [[nodiscard]] const Block& getBlock(int x, int y, int z) const;

    [[nodiscard]] bool isOpaque(int nx, int ny, int nz) const;

    void generateMesh();
    void uploadMesh(); // send data to GPU

    void draw(const std::unordered_map<BlockType, unsigned int> &blockTextureMap) const;

    int chunkX, chunkZ;

private:
    std::array<Block, CHUNK_SIZE*WORLD_HEIGHT*CHUNK_SIZE> blocks;
    MeshData meshData;

    unsigned int VAO = 0, VBO = 0, EBO = 0;

    static int index(const int x, const int y, const int z) {
        return x + CHUNK_SIZE * (z + CHUNK_SIZE * y);
    }
};
