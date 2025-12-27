#include "Chunk.hpp"
#include <iostream>

Chunk::Chunk(const int x, const int z)
    : chunkX(x), chunkZ(z) {
    int index = 0;
    for (int by = 0; by < WORLD_HEIGHT; ++by)
        for (int bz = 0; bz < CHUNK_SIZE; ++bz)
            for (int bx = 0; bx < CHUNK_SIZE; ++bx)
                blocks[index++] = Block(bx, by, bz);
}

Chunk::~Chunk() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (EBO) glDeleteBuffers(1, &EBO);
}

void Chunk::setBlock(int x, int y, int z, BlockType type) {
    blocks[index(x, y, z)].type = type;
}

Block *Chunk::getBlock(int x, int y, int z) { return &blocks[index(x, y, z)]; }
const Block& Chunk::getBlock(int x, int y, int z) const { return blocks[index(x, y, z)]; }

bool Chunk::isOpaque(int nx, int ny, int nz) const {
    if (nx < 0 || ny < 0 || nz < 0 || nx >= CHUNK_SIZE || ny >= CHUNK_SIZE || nz >= CHUNK_SIZE)
        return false; // assume empty outside chunk
    return getBlock(nx, ny, nz).isOpaque();
}

void Chunk::generateMesh() {
    meshData.vertices.clear();
    meshData.indices.clear();

    // Cube vertices local positions
    constexpr float VERTS[8][3] = {
        {0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1},
        {0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1}
    };

    // Faces: two triangles per face, 6 vertices
    constexpr unsigned int FACES[6][6] = {
        {7,6,2,2,3,7}, // front (+Z)
        {5,4,0,0,1,5}, // back (-Z)
        {4,7,3,3,0,4}, // left (-X)
        {6,5,1,1,2,6}, // right (+X)
        {4,5,6,6,7,4}, // top (+Y)
        {3,2,1,1,0,3}  // bottom (-Y)
    };

    // UVs per face vertex (two triangles)
    constexpr float FACE_UVS[6][2] = {
        {0.0f,0.0f},{1.0f,0.0f},{1.0f,1.0f},
        {1.0f,1.0f},{0.0f,1.0f},{0.0f,0.0f}
    };

    auto isOpaque = [&](int nx, int ny, int nz) -> bool {
        if (nx < 0 || ny < 0 || nz < 0 || nx >= CHUNK_SIZE || ny >= CHUNK_SIZE || nz >= CHUNK_SIZE)
            return false; // assume empty outside chunk
        return getBlock(nx, ny, nz)->isOpaque();
    };

    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int y = 0; y < CHUNK_SIZE; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                if (const Block* block = getBlock(x, y, z); !block->isOpaque()) continue;

                for (int f = 0; f < 6; ++f) {
                    bool push = false;
                    switch(f) {
                        case 0: push = !isOpaque(x, y, z+1); break; // front
                        case 1: push = !isOpaque(x, y, z-1); break; // back
                        case 2: push = !isOpaque(x-1, y, z); break; // left
                        case 3: push = !isOpaque(x+1, y, z); break; // right
                        case 4: push = !isOpaque(x, y+1, z); break; // top
                        case 5: push = !isOpaque(x, y-1, z); break; // bottom
                    }
                    if (!push) continue;

                    const unsigned int faceOffset = meshData.vertices.size() / 5; // 5 floats per vertex

                    // Add 6 vertices per face
                    for (int vi = 0; vi < 6; ++vi) {
                        unsigned int vert = FACES[f][vi];
                        meshData.vertices.push_back(VERTS[vert][0] + x);
                        meshData.vertices.push_back(VERTS[vert][1] + y);
                        meshData.vertices.push_back(VERTS[vert][2] + z);
                        meshData.vertices.push_back(FACE_UVS[vi][0]);
                        meshData.vertices.push_back(FACE_UVS[vi][1]);
                    }

                    // Add indices
                    for (int i = 0; i < 6; ++i)
                        meshData.indices.push_back(faceOffset + i);
                }
            }
        }
    }

    std::cout << "Vertices: " << meshData.vertices.size() << " Indices: " << meshData.indices.size() << "\n";
}

void Chunk::uploadMesh() {
    if (VAO == 0) glGenVertexArrays(1, &VAO);
    if (VBO == 0) glGenBuffers(1, &VBO);
    if (EBO == 0) glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, meshData.vertices.size() * sizeof(float), meshData.vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshData.indices.size() * sizeof(unsigned int), meshData.indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Chunk::draw(const std::unordered_map<BlockType, unsigned int> &blockTextureMap) const {
    if (meshData.indices.empty()) return;

    glBindVertexArray(VAO);
    glBindTexture(GL_TEXTURE_2D, blockTextureMap.at(BlockType::Dirt));
    glDrawElements(GL_TRIANGLES, meshData.indices.size(), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}
