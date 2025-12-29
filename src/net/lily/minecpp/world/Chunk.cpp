#define FLOAT static_cast<float>
#define LONG static_cast<long>

#include "Chunk.hpp"
#include <iostream>
#include <memory>

#include "block/BlockRegistry.hpp"
#include "net/lily/minecpp/render/BlockAtlas.hpp"

Chunk::Chunk(const int x, const int z)
    : chunkX(x), chunkZ(z) {
    int index = 0;
    for (int by = 0; by < WORLD_HEIGHT; ++by)
        for (int bz = 0; bz < CHUNK_SIZE; ++bz)
            for (int bx = 0; bx < CHUNK_SIZE; ++bx)
                blocks[index++] = BlockRegistry::createBlock(Material::Air, x + bx, by, z + bz);
}

Chunk::~Chunk() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (EBO) glDeleteBuffers(1, &EBO);
}

void Chunk::setBlock(const int x, const int y, const int z, const Material type) {
    const int indexed = index(x, y, z);
    if (indexed < 0 || indexed >= CHUNK_SIZE * CHUNK_SIZE * WORLD_HEIGHT) {
        std::cerr << "Skipping invalid block " << x << ", " << y << ", " << z << " in chunk " << chunkX << ", " << chunkZ << std::endl;
        return;
    }
    blocks[indexed] = BlockRegistry::createBlock(type, chunkX * CHUNK_SIZE + x, y, chunkZ * CHUNK_SIZE + z);
}

const std::unique_ptr<Block> &Chunk::getBlock(const int x, const int y, const int z) const {
    return blocks[index(x, y, z)];
}

bool Chunk::isOpaque(const int nx, const int ny, const int nz) const {
    if (nx < 0 || ny < 0 || nz < 0 || nx >= CHUNK_SIZE || ny >= WORLD_HEIGHT || nz >= CHUNK_SIZE)
        return false;
    return getBlock(nx, ny, nz)->isOpaque();
}
void Chunk::generateMesh(const BlockAtlas* blockAtlas) {
    meshData.vertices.clear();
    meshData.indices.clear();

    // Cube vertex positions
    constexpr float VERTS[8][3] = {
        {0,0,0},{1,0,0},{1,0,1},{0,0,1},
        {0,1,0},{1,1,0},{1,1,1},{0,1,1}
    };

    // Each face as 4 vertices (quad)
    constexpr unsigned int FACE_VERTS[6][4] = {
        {3,2,6,7}, // front (+Z)
        {1,0,4,5}, // back (-Z)
        {0,3,7,4}, // left (-X)
        {2,1,5,6}, // right (+X)
        {4,7,6,5}, // top (+Y)
        {0,1,2,3}  // bottom (-Y)
    };

    // Each face as two triangles (indices relative to face quad)
    constexpr unsigned int TRIANGLES[2][3] = {
        {0,1,2}, {2,3,0}
    };

    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int y = 0; y < WORLD_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                const auto &block = getBlock(x, y, z);
                if (!block || !block->isOpaque()) continue;
                const Material mat = block->material;

                for (int f = 0; f < 6; ++f) {
                    int nx = x, ny = y, nz = z;
                    switch(f){
                        case 0: nz += 1; break;
                        case 1: nz -= 1; break;
                        case 2: nx -= 1; break;
                        case 3: nx += 1; break;
                        case 4: ny += 1; break;
                        case 5: ny -= 1; break;
                    }
                    if (isOpaque(nx, ny, nz)) continue;

                    auto [uMin, vMin, uMax, vMax] = getTextureUV(blockAtlas, block.get(), f);
                    const unsigned int startIndex = meshData.vertices.size() / 5;

                    for (int i = 0; i < 4; ++i) {
                        const unsigned int vi = FACE_VERTS[f][i];
                        meshData.vertices.push_back(VERTS[vi][0] + static_cast<float>(x));
                        meshData.vertices.push_back(VERTS[vi][1] + static_cast<float>(y));
                        meshData.vertices.push_back(VERTS[vi][2] + static_cast<float>(z));

                        const float u = (i == 0 || i == 3) ? uMax : uMin;
                        const float v = (i == 0 || i == 1) ? vMax : vMin;
                        meshData.vertices.push_back(u);
                        meshData.vertices.push_back(v);
                    }

                    for (const auto t : TRIANGLES) {
                        for (int k = 0; k < 3; ++k) {
                            meshData.indices.push_back(startIndex + t[k]);
                        }
                    }
                }
            }
        }
    }

    std::cout << "Vertices: " << meshData.vertices.size()
              << " Indices: " << meshData.indices.size() << "\n";
}

std::tuple<float,float,float,float> Chunk::getTextureUV(const BlockAtlas* blockAtlas, const Block* block, const int face) {
    return blockAtlas->getBlockUV(block->getTextureName(face));
}

void Chunk::uploadMesh() {
    if (VAO == 0) glGenVertexArrays(1, &VAO);
    if (VBO == 0) glGenBuffers(1, &VBO);
    if (EBO == 0) glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, LONG(meshData.vertices.size()) * sizeof(float), meshData.vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, LONG(meshData.indices.size()) * sizeof(unsigned int), meshData.indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Chunk::draw() const {
    if (meshData.indices.empty()) return;

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, LONG(meshData.indices.size()), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}
