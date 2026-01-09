#include "Chunk.hpp"

#include <mutex>
#include <glm/gtc/type_ptr.hpp>

#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/render/BlockAtlas.hpp"

#define FLOAT static_cast<float>
#define LONG static_cast<long>

std::unique_ptr<Shader> Chunk::boundShader = nullptr;
std::atomic<int> Chunk::loadingMeshes = {0};

Chunk::Chunk(const int x, const int z, const World* world)
    : world(world), mc(world->mc), chunkX(x), chunkZ(z),
      worldPos(x * CHUNK_SIZE, 0, z * CHUNK_SIZE){
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
}

void Chunk::setBlock(const int x, const int y, const int z, const Block block) {
    blocks[index(x, y, z)] = block;
}

Block Chunk::getBlock(const int x, const int y, const int z) const {
    return blocks[index(x, y, z)];
}

void Chunk::testCull(const std::array<Plane, 6> &frustumPlanes, const glm::vec3 cameraPos) const {
    const glm::vec3 relativePos = worldPos - cameraPos;
    culled = !isBoxInFrustum(frustumPlanes, relativePos);
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

constexpr uint32_t packVertice(const unsigned int x, const unsigned int y, const unsigned int z, const uint16_t tileIndex, const uint8_t corner) {
    return x << 26 | y << 17 | z << 12 | tileIndex << 3 | corner;
}

inline void pushVertex(const MeshData &mesh,
                       const unsigned int x, const  unsigned int y, const unsigned int z,
                       const uint16_t tileIndex, const uint8_t cornerFlipped) {
    mesh.vertices.push_back(packVertice(x, y, z, tileIndex, cornerFlipped));
}
inline void pushQuad(const MeshData& mesh,
                     const unsigned int face,
                     const unsigned int x,
                     const unsigned int y,
                     const unsigned int z,
                     const uint16_t tileIndex) {

    static constexpr unsigned int FACE_VERTS[6][4] = {
        {3,2,6,7},{1,0,4,5},{0,3,7,4},
        {2,1,5,6},{4,7,6,5},{0,1,2,3}
    };
    static constexpr unsigned int VERTS[8][3] = {
        {0,0,0},{1,0,0},{1,0,1},{0,0,1},
        {0,1,0},{1,1,0},{1,1,1},{0,1,1}
    };

    const auto& FACE = FACE_VERTS[face];

    const unsigned int *v0 = VERTS[FACE[0]], *v1 = VERTS[FACE[1]], *v2 = VERTS[FACE[2]], *v3 = VERTS[FACE[3]];

    pushVertex(mesh, v0[0] + x, v0[1] + y, v0[2] + z, tileIndex, 3);
    pushVertex(mesh, v1[0] + x, v1[1] + y, v1[2] + z, tileIndex, 2);
    pushVertex(mesh, v2[0] + x, v2[1] + y, v2[2] + z, tileIndex, 1);

    pushVertex(mesh, v2[0] + x, v2[1] + y, v2[2] + z, tileIndex, 1);
    pushVertex(mesh, v3[0] + x, v3[1] + y, v3[2] + z, tileIndex, 0);
    pushVertex(mesh, v0[0] + x, v0[1] + y, v0[2] + z, tileIndex, 3);
}
uint16_t Chunk::getTileIndex(const BlockAtlas& atlas, const Block block, const int face) {
    return static_cast<uint16_t>(atlas.blockAtlasPos
            .at(BlockUtil::getBlockTexture(block, face)).second * atlasTilesPerRow +
            atlas.blockAtlasPos
            .at(BlockUtil::getBlockTexture(block, face)).first);
}

void Chunk::generateMesh(const BlockAtlasData &blockAtlas) const {
    loaded = false;
    loadingMeshes.fetch_add(1, std::memory_order_relaxed);
    meshData.vertices.clear();

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
                                pushQuad(meshData, face, realPos.x, realPos.y, realPos.z, tileIndex);
                            }
                            pendingFaces.erase(it);
                        }
                    }
                    if (block == BLOCK_AIR) continue;
                }

                if (const uint16_t id = blockId(block); id == blockId(BLOCK_AIR) || id == blockId(BLOCK_WATER)) continue;

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

                    pushQuad(meshData, f, x, y, z, getTileIndex(blockAtlas.second, block, f));
                }
            }
        }
    }
}

void Chunk::uploadMesh() const {
    if (VAO == 0) glGenVertexArrays(1, &VAO);
    if (VBO == 0) glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        meshData.vertices.size() * sizeof(uint32_t),
        meshData.vertices.data(),
        GL_STATIC_DRAW
    );

    glVertexAttribIPointer(
        0,
        1,
        GL_UNSIGNED_INT,
        sizeof(uint32_t),
        nullptr
    );
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    initBoundary();
    loaded = true;
    loadingMeshes.fetch_sub(1, std::memory_order_relaxed);
}

void Chunk::initBoundary() const {
    if (boundaryVAO != 0) return;

    std::vector<float> vertices;

    constexpr int step = 2;

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
    // todo this currently segfaults

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
    if (meshData.vertices.empty()) return;

    blockShader->use();
    blockShader->setMat4("model", glm::value_ptr(model));

    glBindVertexArray(VAO);
    glDrawArrays(
        GL_TRIANGLES,
        0,
        static_cast<GLsizei>(meshData.vertices.size())
    );
    glBindVertexArray(0);
}

void Chunk::queueMesh(const BlockAtlasData &blockAtlas) const {
    generateMesh(blockAtlas);

    std::lock_guard lock(Minecraft::meshQueueMutex);
    Minecraft::meshUploadQueue.push_back(MeshUploadJob{shared_from_this()});
}
