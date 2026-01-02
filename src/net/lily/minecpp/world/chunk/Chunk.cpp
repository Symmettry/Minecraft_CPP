#include "Chunk.hpp"

#include <mutex>
#include <glm/gtc/type_ptr.hpp>

#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/render/BlockAtlas.hpp"

#define FLOAT static_cast<float>
#define LONG static_cast<long>

std::unique_ptr<Shader> Chunk::boundShader = nullptr;

Chunk::Chunk(const int x, const int z, const World* world)
    : world(world), chunkX(x), chunkZ(z) {
    blocks.fill(BLOCK_AIR);
    if (const auto it = world->pendingFace4Chunks.find({x, z}); it != world->pendingFace4Chunks.end()) {
        for (auto &[fst, snd] : it->second) {
            pendingFaces[fst].push_back(snd);
        }
        world->pendingFace4Chunks.erase(it);
    }
}

Chunk::~Chunk() {
    if (VAO != 0) glDeleteVertexArrays(1, &VAO);
    if (VBO != 0) glDeleteBuffers(1, &VBO);
    if (EBO != 0) glDeleteBuffers(1, &EBO);
}

void Chunk::setBlock(const int x, const int y, const int z, Block block) {
    if (x < 0 || y < 0 || z < 0 ||
        x >= CHUNK_SIZE || y >= WORLD_HEIGHT || z >= CHUNK_SIZE)
        return;

    blocks[index(x, y, z)] = block;
}

Block Chunk::getBlock(const int x, const int y, const int z) const {
    return blocks[index(x, y, z)];
}

inline uint8_t Chunk::isOpaque(const int x, const int y, const int z) const {
    if (x < 0 || y < 0 || z < 0 ||
        x >= CHUNK_SIZE || y >= WORLD_HEIGHT || z >= CHUNK_SIZE) {
        const int nx = chunkX*CHUNK_SIZE + x, nz = chunkZ*CHUNK_SIZE + z;
        return world->isOpaque(nx, y, nz);
    }

    const Block block = getBlock(x, y, z);
    return BlockUtil::isOpaque(block) ? 1 : 0;
}

inline void pushVertex(const MeshData &mesh,
                       const unsigned int x, const  unsigned int y, const unsigned int z,
                       const uint16_t tileIndex, const uint8_t cornerFlipped) {

    uint8_t flags = 0;
    if (x >= CHUNK_SIZE) flags |= Chunk::OVERFLOW_X;
    if (y >= WORLD_HEIGHT) flags |= Chunk::OVERFLOW_Y;
    if (z >= CHUNK_SIZE) flags |= Chunk::OVERFLOW_Z;

    const unsigned int vertX = std::min(x, 15u);
    const unsigned int vertY = std::min(y, 255u);
    const unsigned int vertZ = std::min(z, 15u);

    mesh.vertices.push_back(Vertex{
        vertX << 12 | vertY << 4 | vertZ,
        flags << 12 | tileIndex << 3 | cornerFlipped
    });
}
inline void pushQuad(const MeshData &mesh,
                     const unsigned int verts[4][3],
                     const unsigned int x, const  unsigned int y, const  unsigned int z,
                     const uint16_t tileIndex) {

    const unsigned int startIndex = static_cast<unsigned int>(mesh.vertices.size());

    for (uint8_t i = 0; i < 4; ++i) {
        uint8_t cornerFlipped;
        switch (i) {
            case 0: cornerFlipped = 3; break;
            case 1: cornerFlipped = 2; break;
            case 2: cornerFlipped = 1; break;
            case 3: cornerFlipped = 0; break;
            default: cornerFlipped = i; break;
        }

        pushVertex(mesh, verts[i][0] + x, verts[i][1] + y, verts[i][2] + z,
                   tileIndex, cornerFlipped);
    }

    for (const auto &tri : Chunk::TRIANGLES) {
        mesh.indices.push_back(startIndex + tri[0]);
        mesh.indices.push_back(startIndex + tri[1]);
        mesh.indices.push_back(startIndex + tri[2]);
    }
}

uint16_t Chunk::getTileIndex(const BlockAtlas& atlas, const Block block, const int face) {
    return static_cast<uint16_t>(atlas.blockAtlasPos
            .at(BlockUtil::getBlockTexture(block, face)).second * atlasTilesPerRow +
            atlas.blockAtlasPos
            .at(BlockUtil::getBlockTexture(block, face)).first);
}

void Chunk::generateMesh(const BlockAtlasData &blockAtlas) const {
    loaded = false;
    meshData.vertices.clear();
    meshData.indices.clear();

    static constexpr unsigned int FACE_VERTS[6][4] = {
        {3,2,6,7},{1,0,4,5},{0,3,7,4},
        {2,1,5,6},{4,7,6,5},{0,1,2,3}
    };
    static constexpr unsigned int VERTS[8][3] = {
        {0,0,0},{1,0,0},{1,0,1},{0,0,1},
        {0,1,0},{1,1,0},{1,1,1},{0,1,1}
    };

    auto addFace = [&](unsigned int xPos, unsigned int yPos, unsigned int zPos,
                       int face, uint16_t tileIndex) {

        unsigned int verts[4][3];
        for (int i = 0; i < 4; ++i) {
            unsigned int vi = FACE_VERTS[face][i];
            verts[i][0] = VERTS[vi][0];
            verts[i][1] = VERTS[vi][1];
            verts[i][2] = VERTS[vi][2];
        }

        pushQuad(meshData, verts, xPos, yPos, zPos, tileIndex);
    };

    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int y = 0; y < WORLD_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {

                const Block block = getBlock(x, y, z);
                glm::ivec3 pos{x, y, z};

                if (!BlockUtil::isOpaque(blockId(block))) {
                    if (x == 0 || x == CHUNK_SIZE-1 || z == 0 || z == CHUNK_SIZE-1) {
                        // lazy-loaded
                        if (const auto it = pendingFaces.find(pos); it != pendingFaces.end()) {
                            for (const auto &pf : it->second) {
                                const uint16_t face = pf & 0x7;
                                const uint16_t tileIndex = pf >> 3;

                                glm::ivec3 offset{0,0,0};
                                switch(face) {
                                    case 0: offset.z = -1; break;
                                    case 1: offset.z = 1; break;
                                    case 2: offset.x = 1; break;
                                    case 3: offset.x = -1; break;
                                    case 4: offset.y = -1; break;
                                    case 5: offset.y = 1; break;
                                    default: break;
                                }

                                const glm::ivec3 realPos = pos + offset;
                                addFace(realPos.x, realPos.y, realPos.z, face, tileIndex);
                            }
                            pendingFaces.erase(it);
                        }
                    }
                    if (block == BLOCK_AIR) continue;
                }

                const uint16_t id = blockId(block);
                if (id == blockId(BLOCK_AIR) || id == blockId(BLOCK_WATER)) continue;

                // Iterate over block faces
                for (int f = 0; f < 6; ++f) {
                    int nx = x, ny = y, nz = z;
                    switch(f) {
                        case 0: ++nz; break;
                        case 1: --nz; break;
                        case 2: --nx; break;
                        case 3: ++nx; break;
                        case 4: ++ny; break;
                        case 5: --ny; break;
                        default: break;
                    }

                    if (const uint8_t k = isOpaque(nx, ny, nz); k != 0) {
                        if (k == 2) { // cross-chunk
                            int neighborChunkX = chunkX;
                            int neighborChunkZ = chunkZ;
                            int localX = nx;
                            int localZ = nz;

                            if (nx < 0) { neighborChunkX -= 1; localX = CHUNK_SIZE - 1; }
                            else if (nx >= CHUNK_SIZE) { neighborChunkX += 1; localX = 0; }

                            if (nz < 0) { neighborChunkZ -= 1; localZ = CHUNK_SIZE - 1; }
                            else if (nz >= CHUNK_SIZE) { neighborChunkZ += 1; localZ = 0; }

                            const uint16_t tileIndex = getTileIndex(blockAtlas.second, block, f);
                            world->addPendingFace(neighborChunkX, neighborChunkZ,
                                                  {localX, ny, localZ},
                                                  static_cast<uint16_t>(tileIndex << 3 | f));
                        }
                        continue;
                    }

                    addFace(x, y, z, f, getTileIndex(blockAtlas.second, block, f));
                }
            }
        }
    }
}

void Chunk::uploadMesh() const {
    if (VAO == 0) glGenVertexArrays(1, &VAO);
    if (VBO == 0) glGenBuffers(1, &VBO);
    if (EBO == 0) glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        meshData.vertices.size() * sizeof(Vertex),
        meshData.vertices.data(),
        GL_STATIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        meshData.indices.size() * sizeof(unsigned int),
        meshData.indices.data(),
        GL_STATIC_DRAW
    );

    glVertexAttribIPointer(
        0,
        1,
        GL_UNSIGNED_SHORT,
        sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, position))
    );
    glEnableVertexAttribArray(0);

    glVertexAttribIPointer(
        1,
        1,
        GL_UNSIGNED_SHORT,
        sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, tileData))
    );
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    initBoundary();
    loaded = true;
}

void Chunk::initBoundary() const {
    if (boundaryVAO != 0) return;

    std::vector<float> vertices;

    // Spacing
    const int step = 2;

    // X-Z faces (bottom and top)
    for (int x = 0; x <= CHUNK_SIZE; x += step) {
        for (int z = 0; z <= CHUNK_SIZE; z += step) {
            // Bottom face
            vertices.push_back(FLOAT(x)); vertices.push_back(0); vertices.push_back(FLOAT(z));
            vertices.push_back(FLOAT(x + step <= CHUNK_SIZE ? x + step : CHUNK_SIZE)); vertices.push_back(0); vertices.push_back(FLOAT(z));

            vertices.push_back(FLOAT(x)); vertices.push_back(0); vertices.push_back(FLOAT(z));
            vertices.push_back(FLOAT(x)); vertices.push_back(0); vertices.push_back(FLOAT(z + step <= CHUNK_SIZE ? z + step : CHUNK_SIZE));

            // Top face
            vertices.push_back(FLOAT(x)); vertices.push_back(FLOAT(WORLD_HEIGHT)); vertices.push_back(FLOAT(z));
            vertices.push_back(FLOAT(x + step <= CHUNK_SIZE ? x + step : CHUNK_SIZE)); vertices.push_back(FLOAT(WORLD_HEIGHT)); vertices.push_back(FLOAT(z));

            vertices.push_back(FLOAT(x)); vertices.push_back(FLOAT(WORLD_HEIGHT)); vertices.push_back(FLOAT(z));
            vertices.push_back(FLOAT(x)); vertices.push_back(FLOAT(WORLD_HEIGHT)); vertices.push_back(FLOAT(z + step <= CHUNK_SIZE ? z + step : CHUNK_SIZE));
        }
    }

    // Y-Z faces (left and right)
    for (int y = 0; y <= WORLD_HEIGHT; y += step) {
        for (int z = 0; z <= CHUNK_SIZE; z += step) {
            // Left face (x=0)
            vertices.push_back(0); vertices.push_back(FLOAT(y)); vertices.push_back(FLOAT(z));
            vertices.push_back(0); vertices.push_back(FLOAT(y + step <= WORLD_HEIGHT ? y + step : WORLD_HEIGHT)); vertices.push_back(FLOAT(z));

            vertices.push_back(0); vertices.push_back(FLOAT(y)); vertices.push_back(FLOAT(z));
            vertices.push_back(0); vertices.push_back(FLOAT(y)); vertices.push_back(FLOAT(z + step <= CHUNK_SIZE ? z + step : CHUNK_SIZE));

            // Right face (x=CHUNK_SIZE)
            vertices.push_back(FLOAT(CHUNK_SIZE)); vertices.push_back(FLOAT(y)); vertices.push_back(FLOAT(z));
            vertices.push_back(FLOAT(CHUNK_SIZE)); vertices.push_back(FLOAT(y + step <= WORLD_HEIGHT ? y + step : WORLD_HEIGHT)); vertices.push_back(FLOAT(z));

            vertices.push_back(FLOAT(CHUNK_SIZE)); vertices.push_back(FLOAT(y)); vertices.push_back(FLOAT(z));
            vertices.push_back(FLOAT(CHUNK_SIZE)); vertices.push_back(FLOAT(y)); vertices.push_back(FLOAT(z + step <= CHUNK_SIZE ? z + step : CHUNK_SIZE));
        }
    }

    // X-Y faces (front and back)
    for (int x = 0; x <= CHUNK_SIZE; x += step) {
        for (int y = 0; y <= WORLD_HEIGHT; y += step) {
            // Front face (z=0)
            vertices.push_back(FLOAT(x)); vertices.push_back(FLOAT(y)); vertices.push_back(0);
            vertices.push_back(FLOAT(x + step <= CHUNK_SIZE ? x + step : CHUNK_SIZE)); vertices.push_back(FLOAT(y)); vertices.push_back(0);

            vertices.push_back(FLOAT(x)); vertices.push_back(FLOAT(y)); vertices.push_back(0);
            vertices.push_back(FLOAT(x)); vertices.push_back(FLOAT(y + step <= WORLD_HEIGHT ? y + step : WORLD_HEIGHT)); vertices.push_back(0);

            // Back face (z=CHUNK_SIZE)
            vertices.push_back(FLOAT(x)); vertices.push_back(FLOAT(y)); vertices.push_back(FLOAT(CHUNK_SIZE));
            vertices.push_back(FLOAT(x + step <= CHUNK_SIZE ? x + step : CHUNK_SIZE)); vertices.push_back(FLOAT(y)); vertices.push_back(FLOAT(CHUNK_SIZE));

            vertices.push_back(FLOAT(x)); vertices.push_back(FLOAT(y)); vertices.push_back(FLOAT(CHUNK_SIZE));
            vertices.push_back(FLOAT(x)); vertices.push_back(FLOAT(y + step <= WORLD_HEIGHT ? y + step : WORLD_HEIGHT)); vertices.push_back(FLOAT(CHUNK_SIZE));
        }
    }

    boundaryVertexCount = vertices.size() / 3;

    glGenVertexArrays(1, &boundaryVAO);
    glGenBuffers(1, &boundaryVBO);

    glBindVertexArray(boundaryVAO);
    glBindBuffer(GL_ARRAY_BUFFER, boundaryVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}


void Chunk::drawBoundaries(const glm::mat4 &projView) const {
    if (boundaryVAO == 0) return;
    if (boundShader == nullptr) {
        std::wcerr << "[WARN] Boundary shader is nullptr" << std::endl;
        return;
    }

    boundShader->use();
    boundShader->setMat4("uMVP", glm::value_ptr(projView));
    glUniform3f(glGetUniformLocation(boundShader->ID, "uColor"), 1.0f, 0.0f, 0.0f);

    glBindVertexArray(boundaryVAO);
    glDrawArrays(GL_LINES, 0, boundaryVertexCount);
    glBindVertexArray(0);
}

void Chunk::draw(const Shader* blockShader, glm::mat4& model) const {
    if (meshData.indices.empty()) return;

    blockShader->use();
    blockShader->setMat4("model", glm::value_ptr(model));

    glBindVertexArray(VAO);
    glDrawElements(
        GL_TRIANGLES,
        LONG(meshData.indices.size()),
        GL_UNSIGNED_INT,
        nullptr
    );
    glBindVertexArray(0);
}

void Chunk::queueMesh(const BlockAtlasData &blockAtlas) const {
    generateMesh(blockAtlas);

    std::lock_guard lock(Minecraft::meshQueueMutex);
    Minecraft::meshUploadQueue.push_back(MeshUploadJob{shared_from_this()});
}
