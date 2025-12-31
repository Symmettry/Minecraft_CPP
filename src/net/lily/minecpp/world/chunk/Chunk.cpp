#include "Chunk.hpp"

#include <iostream>
#include <mutex>
#include <glm/gtc/type_ptr.hpp>

#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/render/BlockAtlas.hpp"
#include "net/lily/minecpp/render/Camera.hpp"
#include "net/lily/minecpp/render/Camera.hpp"

#define FLOAT static_cast<float>
#define LONG static_cast<long>

Shader* Chunk::boundShader;

Chunk::Chunk(const int x, const int z)
    : chunkX(x), chunkZ(z) {
    blocks.fill(BLOCK_AIR);
}

Chunk::~Chunk() {
    if (VAO != 0) glDeleteVertexArrays(1, &VAO);
    if (VBO != 0) glDeleteBuffers(1, &VBO);
    if (EBO != 0) glDeleteBuffers(1, &EBO);
}

void Chunk::setBlock(const int x, const int y, const int z, const Block block) {
    if (x < 0 || y < 0 || z < 0 ||
        x >= CHUNK_SIZE || y >= WORLD_HEIGHT || z >= CHUNK_SIZE)
        return;

    blocks[index(x, y, z)] = block;
}

Block Chunk::getBlock(const int x, const int y, const int z) const {
    return blocks[index(x, y, z)];
}

bool Chunk::isOpaque(const int x, const int y, const int z) const {
    if (x < 0 || y < 0 || z < 0 ||
        x >= CHUNK_SIZE || y >= WORLD_HEIGHT || z >= CHUNK_SIZE)
        return true; // TODO: Queue this x,y,z coordinate and update the mesh accordingly

    const Block block = getBlock(x, y, z);
    return BLOCK_OPAQUE[blockId(block)];
}

void Chunk::generateMesh(const BlockAtlasData &blockAtlas) const {
    loaded = false;
    meshData.vertices.clear();
    meshData.indices.clear();

    constexpr float VERTS[8][3] = {
        {0,0,0},{1,0,0},{1,0,1},{0,0,1},
        {0,1,0},{1,1,0},{1,1,1},{0,1,1}
    };

    constexpr unsigned int FACE_VERTS[6][4] = {
        {3,2,6,7},
        {1,0,4,5},
        {0,3,7,4},
        {2,1,5,6},
        {4,7,6,5},
        {0,1,2,3}
    };

    constexpr unsigned int TRIANGLES[2][3] = {
        {0,1,2}, {2,3,0}
    };

    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int y = 0; y < WORLD_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {

                const Block block = getBlock(x, y, z);
                if (block == BLOCK_AIR) continue;

                const uint16_t id = blockId(block);
                if (!BLOCK_OPAQUE[id]) continue;

                for (int f = 0; f < 6; ++f) {
                    int nx = x;
                    int ny = y;
                    int nz = z;

                    switch (f) {
                        case 0: ++nz; break;
                        case 1: --nz; break;
                        case 2: --nx; break;
                        case 3: ++nx; break;
                        case 4: ++ny; break;
                        case 5: --ny; break;
                        default: break;
                    }

                    if (isOpaque(nx, ny, nz)) continue;

                    const auto [uMin, vMin, uMax, vMax] =
                        blockAtlas.second.getBlockUV(getBlockTexture(id, f));

                    const unsigned int startIndex =
                        meshData.vertices.size() / 5;

                    for (int i = 0; i < 4; ++i) {
                        const unsigned int vi = FACE_VERTS[f][i];

                        meshData.vertices.push_back(VERTS[vi][0] + FLOAT(x));
                        meshData.vertices.push_back(VERTS[vi][1] + FLOAT(y));
                        meshData.vertices.push_back(VERTS[vi][2] + FLOAT(z));

                        const float u = (i == 0 || i == 3) ? uMax : uMin;
                        const float v = (i == 0 || i == 1) ? vMax : vMin;

                        meshData.vertices.push_back(u);
                        meshData.vertices.push_back(v);
                    }

                    for (const auto& tri : TRIANGLES) {
                        meshData.indices.push_back(startIndex + tri[0]);
                        meshData.indices.push_back(startIndex + tri[1]);
                        meshData.indices.push_back(startIndex + tri[2]);
                    }
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
        LONG(meshData.vertices.size() * sizeof(float)),
        meshData.vertices.data(),
        GL_STATIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        LONG(meshData.indices.size() * sizeof(unsigned int)),
        meshData.indices.data(),
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE,
        5 * sizeof(float),
        reinterpret_cast<void*>(3 * sizeof(float))
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
    Minecraft::meshUploadQueue.push_back({this});
}
