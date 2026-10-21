#include "Chunk.hpp"

#include <iostream>
#include <deque>
#include <mutex>
#include <thread>
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include <tracy/Tracy.hpp>

#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/render/BlockAtlas.hpp"

#define FLOAT static_cast<float>
#define LCAST static_cast<long>

std::atomic<int> Chunk::loadingMeshes = {0};

Chunk::Chunk(const int x, const int z, const World* world)
    : world(world), mc(world->mc), chunkX(x), chunkZ(z),
      worldPos(x * CHUNK_SIZE, 0, z * CHUNK_SIZE){
    blocks.fill(BLOCK_AIR);
    biomes.fill(1);
    std::lock_guard lock(world->chunksMutex);
    if (const auto it = world->pendingFace4Chunks.find({x, z}); it != world->pendingFace4Chunks.end()) {
        for (auto &[fst, snd] : it->second) {
            pendingFaces[fst].push_back(snd);
        }
        world->pendingFace4Chunks.erase(it);
    }
}

Chunk::~Chunk() {
    std::cerr << "[Chunk " << chunkX << ',' << chunkZ << "] Destroying on thread "
              << std::this_thread::get_id() << ", GL context=" << glfwGetCurrentContext() << '\n';

    if (glfwGetCurrentContext() == nullptr) {
        std::lock_guard lock(Minecraft::meshQueueMutex);
        Minecraft::meshDeleteQueue.push_back({VAO, VBO});
        return;
    }

    if (VAO != 0) glDeleteVertexArrays(1, &VAO);
    if (VBO != 0) glDeleteBuffers(1, &VBO);
}

void Chunk::setBlock(const int x, const int y, const int z, const Block block) {
    blocks[index(x, y, z)] = block;
}

void Chunk::relightSkylight() {
    for (uint8_t& value : light) value &= 15U;

    std::deque<int> pending;
    const uint8_t skySource = mc->player->dimension == 0 ? 15 : 0;
    for (int localX = 0; localX < CHUNK_SIZE; ++localX) {
        for (int localZ = 0; localZ < CHUNK_SIZE; ++localZ) {
            uint8_t sky = skySource;
            for (int y = WORLD_HEIGHT - 1; y >= 0; --y) {
                const uint8_t opacity = BlockUtil::getSkyLightOpacity(getBlock(localX, y, localZ));
                sky = opacity >= sky ? 0 : static_cast<uint8_t>(sky - opacity);
                const int lightIndex = index(localX, y, localZ);
                light[lightIndex] = static_cast<uint8_t>((light[lightIndex] & 15U) | (sky << 4U));
                if (sky != 0) pending.push_back(lightIndex);
            }
        }
    }

    const auto spreadFromNeighbor = [&](const int localX, const int y, const int localZ,
                                        const uint8_t neighborLight) {
        const uint8_t attenuation = std::max<uint8_t>(1,
            BlockUtil::getSkyLightOpacity(getBlock(localX, y, localZ)));
        const uint8_t propagated = neighborLight > attenuation ? neighborLight - attenuation : 0;
        const int lightIndex = index(localX, y, localZ);
        if (propagated <= (light[lightIndex] >> 4U)) return;
        light[lightIndex] = static_cast<uint8_t>((light[lightIndex] & 15U) | (propagated << 4U));
        pending.push_back(lightIndex);
    };

    const std::shared_ptr<Chunk> west = world->getLoadedChunk(chunkX - 1, chunkZ);
    const std::shared_ptr<Chunk> east = world->getLoadedChunk(chunkX + 1, chunkZ);
    const std::shared_ptr<Chunk> north = world->getLoadedChunk(chunkX, chunkZ - 1);
    const std::shared_ptr<Chunk> south = world->getLoadedChunk(chunkX, chunkZ + 1);
    for (int y = 0; y < WORLD_HEIGHT; ++y) {
        for (int offset = 0; offset < CHUNK_SIZE; ++offset) {
            if (west != nullptr) spreadFromNeighbor(0, y, offset,
                static_cast<uint8_t>(west->getLightData(CHUNK_SIZE - 1, y, offset) >> 4U));
            if (east != nullptr) spreadFromNeighbor(CHUNK_SIZE - 1, y, offset,
                static_cast<uint8_t>(east->getLightData(0, y, offset) >> 4U));
            if (north != nullptr) spreadFromNeighbor(offset, y, 0,
                static_cast<uint8_t>(north->getLightData(offset, y, CHUNK_SIZE - 1) >> 4U));
            if (south != nullptr) spreadFromNeighbor(offset, y, CHUNK_SIZE - 1,
                static_cast<uint8_t>(south->getLightData(offset, y, 0) >> 4U));
        }
    }

    static constexpr int directions[6][3] = {
        {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}
    };
    while (!pending.empty()) {
        const int lightIndex = pending.front();
        pending.pop_front();
        const int localX = lightIndex % CHUNK_SIZE;
        const int localZ = lightIndex / CHUNK_SIZE % CHUNK_SIZE;
        const int y = lightIndex / (CHUNK_SIZE * CHUNK_SIZE);
        const uint8_t source = light[lightIndex] >> 4U;
        if (source <= 1) continue;

        for (const auto& direction : directions) {
            const int neighborX = localX + direction[0];
            const int neighborY = y + direction[1];
            const int neighborZ = localZ + direction[2];
            if (neighborX < 0 || neighborX >= CHUNK_SIZE || neighborY < 0 || neighborY >= WORLD_HEIGHT
                || neighborZ < 0 || neighborZ >= CHUNK_SIZE) continue;
            spreadFromNeighbor(neighborX, neighborY, neighborZ, source);
        }
    }
}

// block light from the emitting blocks (torches, lit furnaces, lava...), spread through the chunk and in from the borders of the neighboring chunks. Used after local edits; chunk data from the server carries its own
void Chunk::relightBlockLight() {
    for (uint8_t& value : light) value &= 0xF0U;
    std::deque<int> pending;
    for (int y = 0; y < WORLD_HEIGHT; ++y) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                const uint8_t emitted = BlockUtil::getLightValue(getBlock(x, y, z));
                if (emitted == 0) continue;
                const int lightIndex = index(x, y, z);
                light[lightIndex] = static_cast<uint8_t>((light[lightIndex] & 0xF0U) | emitted);
                pending.push_back(index(x, y, z));
            }
        }
    }
    const auto spreadFrom = [&](const int x, const int y, const int z, const uint8_t neighborLight) {
        const uint8_t attenuation = std::max<uint8_t>(1, BlockUtil::getSkyLightOpacity(getBlock(x, y, z)));
        const uint8_t propagated = neighborLight > attenuation ? neighborLight - attenuation : 0;
        const int lightIndex = index(x, y, z);
        if (propagated <= (light[lightIndex] & 15U)) return;
        light[lightIndex] = static_cast<uint8_t>((light[lightIndex] & 0xF0U) | propagated);
        pending.push_back(lightIndex);
    };
    const std::shared_ptr<Chunk> west = world->getLoadedChunk(chunkX - 1, chunkZ);
    const std::shared_ptr<Chunk> east = world->getLoadedChunk(chunkX + 1, chunkZ);
    const std::shared_ptr<Chunk> north = world->getLoadedChunk(chunkX, chunkZ - 1);
    const std::shared_ptr<Chunk> south = world->getLoadedChunk(chunkX, chunkZ + 1);
    for (int y = 0; y < WORLD_HEIGHT; ++y) {
        for (int offset = 0; offset < CHUNK_SIZE; ++offset) {
            if (west != nullptr) spreadFrom(0, y, offset, static_cast<uint8_t>(west->getLightData(CHUNK_SIZE - 1, y, offset) & 15U));
            if (east != nullptr) spreadFrom(CHUNK_SIZE - 1, y, offset, static_cast<uint8_t>(east->getLightData(0, y, offset) & 15U));
            if (north != nullptr) spreadFrom(offset, y, 0, static_cast<uint8_t>(north->getLightData(offset, y, CHUNK_SIZE - 1) & 15U));
            if (south != nullptr) spreadFrom(offset, y, CHUNK_SIZE - 1, static_cast<uint8_t>(south->getLightData(offset, y, 0) & 15U));
        }
    }
    static constexpr int directions[6][3] = {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
    while (!pending.empty()) {
        const int lightIndex = pending.front();
        pending.pop_front();
        const int x = lightIndex % CHUNK_SIZE;
        const int z = lightIndex / CHUNK_SIZE % CHUNK_SIZE;
        const int y = lightIndex / (CHUNK_SIZE * CHUNK_SIZE);
        const uint8_t source = light[lightIndex] & 15U;
        if (source <= 1) continue;
        for (const auto& direction : directions) {
            const int nx = x + direction[0], ny = y + direction[1], nz = z + direction[2];
            if (nx < 0 || nx >= CHUNK_SIZE || ny < 0 || ny >= WORLD_HEIGHT || nz < 0 || nz >= CHUNK_SIZE) continue;
            spreadFrom(nx, ny, nz, source);
        }
    }
}

uint32_t Chunk::relightSkylightSections() {
    const std::array<uint8_t, CHUNK_SIZE * CHUNK_SIZE * WORLD_HEIGHT> lightBefore = light;
    relightSkylight();
    relightBlockLight();
    uint32_t mask = 0;
    constexpr int layer = CHUNK_SIZE * CHUNK_SIZE;
    for (int y = 0; y < WORLD_HEIGHT; ++y) {
        if (std::memcmp(lightBefore.data() + y * layer, light.data() + y * layer, layer) == 0) continue;
        // a face reads the light of the blocks around it, one block either side
        for (int section = std::max(y - 1, 0) / CHUNK_SIZE;
             section <= std::min(y + 1, WORLD_HEIGHT - 1) / CHUNK_SIZE; ++section)
            mask |= 1U << section;
    }
    return mask;
}

void Chunk::initializeEmptySectionSkyLight(const bool hasSky) {
    std::array<int, CHUNK_SIZE * CHUNK_SIZE> heightMap{};
    if (hasSky) {
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                int height = 0;
                for (int y = WORLD_HEIGHT - 1; y >= 0; --y) {
                    if (BlockUtil::getSkyLightOpacity(getBlock(x, y, z)) == 0) continue;
                    height = y + 1;
                    break;
                }
                heightMap[z * CHUNK_SIZE + x] = height;
            }
        }
    }

    for (int section = 0; section < WORLD_HEIGHT / 16; ++section) {
        if (sectionPresent[section]) continue;
        const int minY = section * 16;
        for (int y = minY; y < minY + 16; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    const int lightIndex = index(x, y, z);
                    const uint8_t sky = hasSky && y >= heightMap[z * CHUNK_SIZE + x] ? 15 : 0;
                    light[lightIndex] = static_cast<uint8_t>((light[lightIndex] & 15U) | (sky << 4U));
                }
            }
        }
    }
}

Block Chunk::getBlock(const int x, const int y, const int z) const {
    return blocks[index(x, y, z)];
}

void Chunk::testCull(const std::array<Plane, 6> &frustumPlanes, const glm::vec3 cameraPos) const {
    const glm::vec3 relativePos = worldPos - cameraPos;
    // visibility is reused for small camera changes. The padding makes those cached results conservative so a section enters the list before its geometry reaches the actual view boundary
    constexpr float cullPadding = 3.0f;
    const glm::vec3 paddedChunkPos = relativePos - glm::vec3(cullPadding);
    constexpr glm::vec3 paddedChunkSize(
        CHUNK_SIZE + cullPadding * 2.0f,
        WORLD_HEIGHT + cullPadding * 2.0f,
        CHUNK_SIZE + cullPadding * 2.0f);
    culled = !isBoxInFrustum(frustumPlanes, paddedChunkPos, paddedChunkSize);
    visibleSections = 0;
    if (culled) return;

    constexpr glm::vec3 sectionSize(CHUNK_SIZE + cullPadding * 2.0f);
    for (int section = 0; section < WORLD_HEIGHT / CHUNK_SIZE; ++section) {
        if (uploadedSectionVertexCounts[section] == 0
            && uploadedTranslucentSectionVertexCounts[section] == 0) continue;
        const glm::vec3 sectionPos = relativePos - glm::vec3(cullPadding)
            + glm::vec3(0.0f, static_cast<float>(section * CHUNK_SIZE), 0.0f);
        if (isBoxInFrustum(frustumPlanes, sectionPos, sectionSize)) {
            visibleSections |= static_cast<uint16_t>(1U << section);
        }
    }
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

uint8_t Chunk::occludesAmbientLight(const int x, const int y, const int z) const {
    if (x < 0 || x >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) {
        const int nx = chunkX * CHUNK_SIZE + x;
        const int nz = chunkZ * CHUNK_SIZE + z;
        return BlockUtil::occludesAmbientLight(world->getBlockAt(nx, y, nz)) ? 1 : 0;
    }
    if (y < 0 || y >= WORLD_HEIGHT) return 0;
    return BlockUtil::occludesAmbientLight(getBlock(x, y, z)) ? 1 : 0;
}

uint32_t Chunk::getLightData(const int x, const int y, const int z) const {
    if (y < 0) return 0;
    if (y >= WORLD_HEIGHT) return 15U << 4;
    const auto rawLight = [&](const int sampleX, const int sampleY, const int sampleZ) {
        if (sampleY < 0) return 0U;
        if (sampleY >= WORLD_HEIGHT) return 15U << 4;
        if (sampleX >= 0 && sampleX < CHUNK_SIZE && sampleZ >= 0 && sampleZ < CHUNK_SIZE) {
            return static_cast<uint32_t>(light[index(sampleX, sampleY, sampleZ)]);
        }
        const int worldX = chunkX * CHUNK_SIZE + sampleX;
        const int worldZ = chunkZ * CHUNK_SIZE + sampleZ;
        const std::shared_ptr<Chunk> neighbor = world->getLoadedChunkAt(worldX, worldZ);
        if (neighbor == nullptr) return 15U << 4;
        return static_cast<uint32_t>(neighbor->light[
            index(World::toLocalX(worldX), sampleY, World::toLocalZ(worldZ))]);
    };

    const uint32_t ownLight = rawLight(x, y, z);
    const int worldX = chunkX * CHUNK_SIZE + x;
    const int worldZ = chunkZ * CHUNK_SIZE + z;
    if (blockId(world->getBlockAt(worldX, y, worldZ)) != blockId(BLOCK_BARRIER)) return ownLight;

    // blockBarrier is translucent and therefore uses the brightest neighboring value in vanilla
    static constexpr int neighbors[6][3]{
        {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}
    };
    uint32_t block = ownLight & 15U;
    uint32_t sky = ownLight >> 4U;
    for (const auto& offset : neighbors) {
        const uint32_t light = rawLight(x + offset[0], y + offset[1], z + offset[2]);
        block = std::max(block, light & 15U);
        sky = std::max(sky, light >> 4U);
    }
    return block | (sky << 4U);
}

constexpr uint32_t packVertice(const unsigned int x, const unsigned int y, const unsigned int z,
                               const uint16_t tileIndex, const uint8_t height, const bool lowerTop) {
    return static_cast<uint32_t>(lowerTop) << 31 | x << 26 | y << 17 | z << 12 | tileIndex << 3 | height;
}

inline void pushVertex(std::vector<Vertex>& vertices,
                       const unsigned int x, const  unsigned int y, const unsigned int z,
                       const uint16_t tileIndex, const uint8_t height, const bool lowerTop,
                       const uint32_t tint, const uint32_t shade, const uint32_t modelOffset = 0,
                       const uint32_t modelUv = 0) {
    vertices.push_back({packVertice(x, y, z, tileIndex, height, lowerTop), tint, shade,
        modelOffset, modelUv});
}
inline void pushQuad(std::vector<Vertex>& vertices,
                     const unsigned int face,
                     const unsigned int x,
                     const unsigned int y,
                     const unsigned int z,
                     const uint16_t tileIndex,
                     const uint8_t height = 0,
                     const uint32_t tint = 0, const uint32_t shade = 255) {

    static constexpr unsigned int FACE_VERTS[6][4] = {
        {3,2,6,7},{1,0,4,5},{0,3,7,4},
        {2,1,5,6},{4,7,6,5},{0,1,2,3}
    };
    static constexpr unsigned int VERTS[8][3] = {
        {0,0,0},{1,0,0},{1,0,1},{0,0,1},
        {0,1,0},{1,1,0},{1,1,1},{0,1,1}
    };

    const auto& FACE = FACE_VERTS[face];
    const uint32_t faceShade = shade | face << 16;

    const unsigned int *v0 = VERTS[FACE[0]], *v1 = VERTS[FACE[1]], *v2 = VERTS[FACE[2]], *v3 = VERTS[FACE[3]];

    pushVertex(vertices, v0[0] + x, v0[1] + y, v0[2] + z, tileIndex, height, v0[1] == 1, tint, faceShade);
    pushVertex(vertices, v1[0] + x, v1[1] + y, v1[2] + z, tileIndex, height, v1[1] == 1, tint, faceShade);
    pushVertex(vertices, v2[0] + x, v2[1] + y, v2[2] + z, tileIndex, height, v2[1] == 1, tint, faceShade);

    pushVertex(vertices, v2[0] + x, v2[1] + y, v2[2] + z, tileIndex, height, v2[1] == 1, tint, faceShade);
    pushVertex(vertices, v3[0] + x, v3[1] + y, v3[2] + z, tileIndex, height, v3[1] == 1, tint, faceShade);
    pushVertex(vertices, v0[0] + x, v0[1] + y, v0[2] + z, tileIndex, height, v0[1] == 1, tint, faceShade);
}

inline void pushFluidFace(std::vector<Vertex>& vertices, const unsigned int x, const unsigned int y,
                          const unsigned int z, const unsigned int face, const uint16_t tile,
                          const std::array<std::array<uint8_t, 3>, 4>& points,
                          const std::array<std::array<uint8_t, 2>, 4>& uvs,
                          const std::array<uint32_t, 4>& lighting) {
    static constexpr unsigned int order[6] = {0, 1, 2, 2, 3, 0};
    for (const unsigned int corner : order) {
        unsigned int coordinates[3]{x, y, z};
        uint32_t fractions = 0;
        for (unsigned int axis = 0; axis < 3; ++axis) {
            const uint8_t unit = points[corner][axis];
            if (axis == 1) continue;
            if (unit == 16) ++coordinates[axis];
            else fractions |= static_cast<uint32_t>(unit) << (axis * 4U);
        }
        fractions |= 1U << 31U | static_cast<uint32_t>(points[corner][1]) << 12U;
        const uint32_t modelUv = 1U << 10U | uvs[corner][0]
            | static_cast<uint32_t>(uvs[corner][1]) << 5U;
        pushVertex(vertices, coordinates[0], coordinates[1], coordinates[2], tile, 0, false, 0,
            lighting[corner] | face << 16U, fractions, modelUv);
    }
}

enum class HalfRange : uint8_t { Full, Lower, Upper };

inline void pushCuboid(std::vector<Vertex>& vertices, const unsigned int x, const unsigned int y,
                       const unsigned int z, const std::array<uint16_t, 6>& tiles,
                       const uint32_t tint, const std::array<bool, 6>& faces,
                       const HalfRange xRange, const HalfRange yRange, const HalfRange zRange,
                       const std::array<std::array<uint32_t, 4>, 6>& lighting) {
    static constexpr unsigned int faceVertices[6][4] = {
        {3,2,6,7},{1,0,4,5},{0,3,7,4},
        {2,1,5,6},{4,7,6,5},{0,1,2,3}
    };
    static constexpr unsigned int cubeVertices[8][3] = {
        {0,0,0},{1,0,0},{1,0,1},{0,0,1},
        {0,1,0},{1,1,0},{1,1,1},{0,1,1}
    };
    static constexpr unsigned int order[6] = {0, 1, 2, 2, 3, 0};
    const uint8_t height = yRange == HalfRange::Lower ? 4 : 0;

    for (unsigned int face = 0; face < 6; ++face) {
        if (!faces[face]) continue;
        for (const unsigned int corner : order) {
            const unsigned int* vertex = cubeVertices[faceVertices[face][corner]];
            uint32_t packedShade = lighting[face][corner] | face << 16U;
            if (xRange == HalfRange::Lower && vertex[0] == 1) packedShade |= 1U << 20U;
            else if (xRange == HalfRange::Upper && vertex[0] == 0) packedShade |= 2U << 20U;
            if (zRange == HalfRange::Lower && vertex[2] == 1) packedShade |= 1U << 22U;
            else if (zRange == HalfRange::Upper && vertex[2] == 0) packedShade |= 2U << 22U;
            if (yRange == HalfRange::Upper && vertex[1] == 0) packedShade |= 1U << 24U;
            pushVertex(vertices, vertex[0] + x, vertex[1] + y, vertex[2] + z,
                tiles[face], height, vertex[1] == 1, tint, packedShade);
        }
    }
}

inline void pushModelCuboid(std::vector<Vertex>& vertices, const unsigned int x, const unsigned int y,
                            const unsigned int z, const std::array<uint16_t, 6>& tiles,
                            const uint32_t tint, const std::array<bool, 6>& faces,
                            const std::array<uint8_t, 3>& minimum,
                            const std::array<uint8_t, 3>& maximum,
                            const std::array<std::array<uint32_t, 4>, 6>& lighting,
                            const uint8_t topUvTurns = 0, const bool flipSideU = false,
                            const uint8_t modelFacing = 255, const uint16_t faceUvTurns = 0) {
    static constexpr unsigned int faceVertices[6][4] = {
        {3,2,6,7},{1,0,4,5},{0,3,7,4},
        {2,1,5,6},{4,7,6,5},{0,1,2,3}
    };
    static constexpr unsigned int cubeVertices[8][3] = {
        {0,0,0},{1,0,0},{1,0,1},{0,0,1},
        {0,1,0},{1,1,0},{1,1,1},{0,1,1}
    };
    static constexpr unsigned int order[6] = {0, 1, 2, 2, 3, 0};
    for (unsigned int face = 0; face < 6; ++face) {
        if (!faces[face]) continue;
        for (const unsigned int corner : order) {
            const unsigned int* vertex = cubeVertices[faceVertices[face][corner]];
            unsigned int coordinates[3]{x, y, z};
            uint32_t fractions = 0;
            for (unsigned int axis = 0; axis < 3; ++axis) {
                const uint8_t unit = vertex[axis] == 0 ? minimum[axis] : maximum[axis];
                if (unit == 16) ++coordinates[axis];
                else fractions |= static_cast<uint32_t>(unit) << (axis * 4U);
            }
            const uint8_t units[3]{
                vertex[0] == 0 ? minimum[0] : maximum[0],
                vertex[1] == 0 ? minimum[1] : maximum[1],
                vertex[2] == 0 ? minimum[2] : maximum[2],
            };
            uint8_t u = 0;
            uint8_t v = 0;
            uint8_t textureFace = static_cast<uint8_t>(face);
            uint8_t textureUnits[3]{units[0], units[1], units[2]};
            if (modelFacing <= 5) {
                switch (modelFacing) {
                    case 0:
                        textureUnits[1] = units[2]; textureUnits[2] = units[1];
                        textureFace = std::array<uint8_t, 6>{4, 5, 2, 3, 0, 1}[face];
                        break;
                    case 1:
                        textureUnits[1] = units[2]; textureUnits[2] = static_cast<uint8_t>(16 - units[1]);
                        textureFace = std::array<uint8_t, 6>{4, 5, 2, 3, 1, 0}[face];
                        break;
                    case 3:
                        textureUnits[0] = static_cast<uint8_t>(16 - units[0]);
                        textureUnits[2] = static_cast<uint8_t>(16 - units[2]);
                        textureFace = std::array<uint8_t, 6>{1, 0, 3, 2, 4, 5}[face];
                        break;
                    case 4:
                        textureUnits[0] = static_cast<uint8_t>(16 - units[2]); textureUnits[2] = units[0];
                        textureFace = std::array<uint8_t, 6>{2, 3, 1, 0, 4, 5}[face];
                        break;
                    case 5:
                        textureUnits[0] = units[2]; textureUnits[2] = static_cast<uint8_t>(16 - units[0]);
                        textureFace = std::array<uint8_t, 6>{3, 2, 0, 1, 4, 5}[face];
                        break;
                    default: break;
                }
            }
            switch (textureFace) {
                case 0: u = textureUnits[0]; v = 16 - textureUnits[1]; break;
                case 1: u = 16 - textureUnits[0]; v = 16 - textureUnits[1]; break;
                case 2: u = textureUnits[2]; v = 16 - textureUnits[1]; break;
                case 3: u = 16 - textureUnits[2]; v = 16 - textureUnits[1]; break;
                case 4: u = textureUnits[2]; v = 16 - textureUnits[0]; break;
                case 5: u = textureUnits[0]; v = 16 - textureUnits[2]; break;
                default: break;
            }
            if (flipSideU && face < 4) u = static_cast<uint8_t>(16 - u);
            if (face == 4) {
                for (uint8_t turn = 0; turn < (topUvTurns & 3); ++turn) {
                    const uint8_t rotatedU = static_cast<uint8_t>(16 - v);
                    v = u;
                    u = rotatedU;
                }
            }
            for (uint8_t turn = 0; turn < (faceUvTurns >> (face * 2U) & 3U); ++turn) {
                const uint8_t rotatedU = static_cast<uint8_t>(16 - v);
                v = u;
                u = rotatedU;
            }
            const uint32_t modelUv = 1U << 10U | static_cast<uint32_t>(u)
                | static_cast<uint32_t>(v) << 5U;
            pushVertex(vertices, coordinates[0], coordinates[1], coordinates[2], tiles[face], 0,
                false, tint, lighting[face][corner] | face << 16U, fractions, modelUv);
        }
    }
}

inline void pushQuad(std::vector<Vertex>& vertices,
                     const unsigned int face,
                     const unsigned int x,
                     const unsigned int y,
                     const unsigned int z,
                     const uint16_t tileIndex,
                     const uint8_t height,
                     const uint32_t tint,
                     const std::array<uint32_t, 4>& shade) {
    static constexpr unsigned int FACE_VERTS[6][4] = {
        {3,2,6,7},{1,0,4,5},{0,3,7,4},
        {2,1,5,6},{4,7,6,5},{0,1,2,3}
    };
    static constexpr unsigned int VERTS[8][3] = {
        {0,0,0},{1,0,0},{1,0,1},{0,0,1},
        {0,1,0},{1,1,0},{1,1,1},{0,1,1}
    };
    static constexpr unsigned int order[6] = {0, 1, 2, 2, 3, 0};
    const auto& faceVertices = FACE_VERTS[face];
    for (const unsigned int corner : order) {
        const unsigned int* vertex = VERTS[faceVertices[corner]];
        pushVertex(vertices, vertex[0] + x, vertex[1] + y, vertex[2] + z,
            tileIndex, height, vertex[1] == 1, tint, shade[corner] | face << 16);
    }
}

inline void pushCrossPlane(std::vector<Vertex>& vertices, const unsigned int x, const unsigned int y,
                           const unsigned int z, const uint16_t tileIndex, const uint32_t tint, const uint32_t shade,
                           const uint32_t renderOffset,
                           const unsigned int ax, const unsigned int az,
                           const unsigned int bx, const unsigned int bz) {
    const auto pushSide = [&](const bool reverse) {
        const unsigned int firstX = reverse ? bx : ax;
        const unsigned int firstZ = reverse ? bz : az;
        const unsigned int secondX = reverse ? ax : bx;
        const unsigned int secondZ = reverse ? az : bz;
        const uint32_t packedShade = shade | renderOffset;
        const uint8_t firstCorner = static_cast<uint8_t>(firstX | firstZ << 1U);
        const uint8_t secondCorner = static_cast<uint8_t>(secondX | secondZ << 1U);
        pushVertex(vertices, x + firstX, y, z + firstZ, tileIndex, firstCorner, false, tint, packedShade);
        pushVertex(vertices, x + secondX, y, z + secondZ, tileIndex, secondCorner, false, tint, packedShade);
        pushVertex(vertices, x + secondX, y + 1, z + secondZ, tileIndex, secondCorner, false, tint, packedShade);
        pushVertex(vertices, x + secondX, y + 1, z + secondZ, tileIndex, secondCorner, false, tint, packedShade);
        pushVertex(vertices, x + firstX, y + 1, z + firstZ, tileIndex, firstCorner, false, tint, packedShade);
        pushVertex(vertices, x + firstX, y, z + firstZ, tileIndex, firstCorner, false, tint, packedShade);
    };
    pushSide(false);
    pushSide(true);
}

inline void pushCross(std::vector<Vertex>& vertices, const unsigned int x, const unsigned int y,
                      const unsigned int z, const uint16_t tileIndex, const uint32_t tint,
                      const uint32_t shade, const uint32_t renderOffset) {
    pushCrossPlane(vertices, x, y, z, tileIndex, tint, shade, renderOffset, 0, 0, 1, 1);
    pushCrossPlane(vertices, x, y, z, tileIndex, tint, shade, renderOffset, 0, 1, 1, 0);
}

// a flat or sloped quad whose corners are given in 1/16 block units relative to the block (0..32); uvs are in 1/16 of the tile. Drawn from both sides
inline void pushModelPlane(std::vector<Vertex>& vertices, const unsigned int x, const unsigned int y,
                           const unsigned int z, const uint16_t tile, const uint32_t tint,
                           const uint32_t shade, const unsigned int face,
                           const std::array<std::array<uint8_t, 3>, 4>& corners,
                           const std::array<std::array<uint8_t, 2>, 4>& uvs) {
    static constexpr unsigned int forward[6] = {0, 1, 2, 2, 3, 0};
    static constexpr unsigned int backward[6] = {0, 3, 2, 2, 1, 0};
    for (const auto* order : {forward, backward}) {
        for (int index = 0; index < 6; ++index) {
            const unsigned int corner = order[index];
            unsigned int coordinates[3]{x, y, z};
            uint32_t fractions = 0;
            for (unsigned int axis = 0; axis < 3; ++axis) {
                const unsigned int unit = corners[corner][axis];
                coordinates[axis] += unit / 16U;
                fractions |= static_cast<uint32_t>(unit % 16U) << (axis * 4U);
            }
            const uint32_t modelUv = 1U << 10U | static_cast<uint32_t>(uvs[corner][0])
                | static_cast<uint32_t>(uvs[corner][1]) << 5U;
            pushVertex(vertices, coordinates[0], coordinates[1], coordinates[2], tile, 0, false,
                tint, shade | face << 16U, fractions, modelUv);
        }
    }
}

// the plant flag with (almost) no random offset: the shader only takes the cross-plant path with this bit set, otherwise it reads the corner index as a block height and squashes the texture
constexpr uint32_t plantNoOffset = 1U << 19U | 7U << 20U | 15U << 24U | 8U << 28U;

uint32_t plantRenderOffset(const int x, const int z, const bool offsetY) {
    uint64_t random = static_cast<uint64_t>(static_cast<int64_t>(x) * 3129871LL)
        ^ static_cast<uint64_t>(static_cast<int64_t>(z) * 116129781LL);
    random = random * random * 42317861ULL + random * 11ULL;
    const uint32_t randomX = static_cast<uint32_t>((random >> 16U) & 15U);
    const uint32_t randomY = offsetY ? static_cast<uint32_t>((random >> 20U) & 15U) : 15U;
    const uint32_t randomZ = static_cast<uint32_t>((random >> 24U) & 15U);
    return 1U << 19U | randomX << 20U | randomY << 24U | randomZ << 28U;
}

static std::pair<float, float> biomeClimate(uint8_t biome) {
    if (biome >= 128) biome -= 128;
    switch (biome) {
        case 2: case 17: return {2.0f, 0.0f};
        case 3: case 20: case 25: case 34: return {0.2f, 0.3f};
        case 4: case 18: case 27: case 29: return {0.7f, 0.8f};
        case 5: case 19: return {0.25f, 0.8f};
        case 6: return {0.8f, 0.9f};
        case 10: case 11: case 12: case 13: return {0.0f, 0.5f};
        case 14: case 15: return {0.9f, 1.0f};
        case 21: case 22: return {0.95f, 0.9f};
        case 23: return {0.95f, 0.8f};
        case 26: return {0.05f, 0.3f};
        case 28: return {0.6f, 0.6f};
        case 30: case 31: return {-0.5f, 0.4f};
        case 32: case 33: return {0.3f, 0.8f};
        case 35: case 36: return {1.2f, 0.0f};
        case 37: case 38: case 39: return {2.0f, 0.0f};
        case 1: case 16: return {0.8f, 0.4f};
        default: return {0.5f, 0.5f};
    }
}

static uint32_t tintData(const Block block, const int face, const uint8_t biome,
                         const uint16_t overlayTile = 0) {
    uint32_t renderLayer = 0;
    switch (blockId(block)) {
        case blockId(BLOCK_TALLGRASS): case blockId(BLOCK_DOUBLE_PLANT): case blockId(BLOCK_VINE):
            renderLayer = 1U << 31U;
            break;
        default: break;
    }
    uint32_t kind = 0;
    switch (blockId(block)) {
        case blockId(BLOCK_GRASS):
            if (face == 4) kind = 1;
            else if (face < 4) kind = 5;
            break;
        case blockId(BLOCK_TALLGRASS):
            switch (blockMeta(block)) {
                case 1: kind = 1; break;
                case 2: kind = 1; break;
                default: break;
            }
            break;
        case blockId(BLOCK_DOUBLE_PLANT):
            kind = 1;
            break;
        case blockId(BLOCK_LEAVES):
            switch (blockMeta(block) & 3) {
                case 1: kind = 3; break;
                case 2: kind = 4; break;
                default: kind = 2; break;
            }
            break;
        case blockId(BLOCK_LEAVES2):
        case blockId(BLOCK_VINE):
            kind = 2;
            break;
        default:
            break;
    }
    if (kind == 0 || kind == 3 || kind == 4) return renderLayer | kind;
    auto [temperature, rainfall] = biomeClimate(biome);
    temperature = std::clamp(temperature, 0.0f, 1.0f);
    rainfall = std::clamp(rainfall, 0.0f, 1.0f) * temperature;
    const uint32_t colorX = static_cast<uint32_t>((1.0f - temperature) * 255.0f);
    const uint32_t colorY = static_cast<uint32_t>((1.0f - rainfall) * 255.0f);
    return renderLayer | kind | colorX << 3 | colorY << 11
        | static_cast<uint32_t>(overlayTile) << 19;
}

constexpr uint8_t getRenderHeight(const Block block) {
    if (blockId(block) != blockId(BLOCK_SNOW_LAYER)) return 0;
    const uint8_t layers = blockMeta(block) + 1;
    return layers < 8 ? layers : 0;
}
uint16_t Chunk::getTileIndex(const BlockAtlas& atlas, const Block block, const int face) {
    static constexpr int geometryFaceToBlockFace[6]{1, 0, 3, 2, 4, 5};
    const int blockFace = face >= 0 && face < 6 ? geometryFaceToBlockFace[face] : face;
    const auto& [x, y] = atlas.blockAtlasPos.at(BlockUtil::getBlockTexture(block, blockFace));
    return static_cast<uint16_t>(y * atlasTilesPerRow + x);
}

static uint16_t getNamedTileIndex(const BlockAtlas& atlas, const std::string& texture) {
    const char* key = BlockUtil::getAtlasTexturePtr(atlas, texture);
    const auto& [x, y] = atlas.blockAtlasPos.at(key);
    return static_cast<uint16_t>(y * atlasTilesPerRow + x);
}

// like getNamedTileIndex, but a texture missing from the atlas draws nothing instead of throwing
static bool tryNamedTile(const BlockAtlas& atlas, const std::string& texture, uint16_t& tile) {
    for (const auto& [name, position] : atlas.blockAtlasPos) {
        if (texture == name) {
            tile = static_cast<uint16_t>(position.second * atlasTilesPerRow + position.first);
            return true;
        }
    }
    return false;
}

static const char* doublePlantTexture(const uint8_t variant, const bool upper) {
    switch (variant) {
        case 0: return upper ? "double_plant_sunflower_top" : "double_plant_sunflower_bottom";
        case 1: return upper ? "double_plant_syringa_top" : "double_plant_syringa_bottom";
        case 2: return upper ? "double_plant_grass_top" : "double_plant_grass_bottom";
        case 3: return upper ? "double_plant_fern_top" : "double_plant_fern_bottom";
        case 4: return upper ? "double_plant_rose_top" : "double_plant_rose_bottom";
        case 5: return upper ? "double_plant_paeonia_top" : "double_plant_paeonia_bottom";
        default: return upper ? "double_plant_grass_top" : "double_plant_grass_bottom";
    }
}

static uint32_t faceLighting(const Chunk& chunk, const int x, const int y, const int z, const int face) {
    int lx = x, ly = y, lz = z;
    float direction = 1.0f;
    switch (face) {
        case 0: ++lz; direction = 0.8f; break;
        case 1: --lz; direction = 0.8f; break;
        case 2: --lx; direction = 0.6f; break;
        case 3: ++lx; direction = 0.6f; break;
        case 4: ++ly; direction = 1.0f; break;
        case 5: --ly; direction = 0.5f; break;
        default: break;
    }
    const uint32_t directionByte = static_cast<uint32_t>(direction * 255.0f);
    return directionByte | chunk.getLightData(lx, ly, lz) << 8;
}

static std::array<uint32_t, 4> smoothFaceLighting(const Chunk& chunk, const int x, const int y,
                                                  const int z, const int face) {
    static constexpr int normals[6][3] = {
        {0, 0, 1}, {0, 0, -1}, {-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}
    };
    static constexpr int vertices[6][4][3] = {
        {{0,0,1},{1,0,1},{1,1,1},{0,1,1}}, {{1,0,0},{0,0,0},{0,1,0},{1,1,0}},
        {{0,0,0},{0,0,1},{0,1,1},{0,1,0}}, {{1,0,1},{1,0,0},{1,1,0},{1,1,1}},
        {{0,1,0},{0,1,1},{1,1,1},{1,1,0}}, {{0,0,0},{1,0,0},{1,0,1},{0,0,1}}
    };
    static constexpr float directionShade[6] = {0.8f, 0.8f, 0.6f, 0.6f, 1.0f, 0.5f};

    int tangentA = 0;
    int tangentB = 1;
    switch (face) {
        case 0: case 1: tangentA = 0; tangentB = 1; break;
        case 2: case 3: tangentA = 2; tangentB = 1; break;
        case 4: case 5: tangentA = 0; tangentB = 2; break;
        default: break;
    }

    const int baseX = x + normals[face][0];
    const int baseY = y + normals[face][1];
    const int baseZ = z + normals[face][2];
    const uint32_t centerLight = chunk.getLightData(baseX, baseY, baseZ);
    std::array<uint32_t, 4> result{};
    for (int corner = 0; corner < 4; ++corner) {
        int sideA[3]{baseX, baseY, baseZ};
        int sideB[3]{baseX, baseY, baseZ};
        const int signA = vertices[face][corner][tangentA] == 0 ? -1 : 1;
        const int signB = vertices[face][corner][tangentB] == 0 ? -1 : 1;
        sideA[tangentA] += signA;
        sideB[tangentB] += signB;
        int diagonal[3]{sideA[0], sideA[1], sideA[2]};
        diagonal[tangentB] += signB;

        const bool opaqueA = chunk.occludesAmbientLight(sideA[0], sideA[1], sideA[2]) != 0;
        const bool opaqueB = chunk.occludesAmbientLight(sideB[0], sideB[1], sideB[2]) != 0;
        const bool opaqueDiagonal = opaqueA && opaqueB
            ? true : chunk.occludesAmbientLight(diagonal[0], diagonal[1], diagonal[2]) != 0;
        uint32_t lightA = chunk.getLightData(sideA[0], sideA[1], sideA[2]);
        uint32_t lightB = chunk.getLightData(sideB[0], sideB[1], sideB[2]);
        uint32_t lightDiagonal = opaqueA && opaqueB
            ? lightA : chunk.getLightData(diagonal[0], diagonal[1], diagonal[2]);
        if (lightA == 0) lightA = centerLight;
        if (lightB == 0) lightB = centerLight;
        if (lightDiagonal == 0) lightDiagonal = centerLight;
        const uint32_t blockLight = ((centerLight & 15U) + (lightA & 15U)
            + (lightB & 15U) + (lightDiagonal & 15U)) / 4U;
        const uint32_t skyLight = ((centerLight >> 4U) + (lightA >> 4U)
            + (lightB >> 4U) + (lightDiagonal >> 4U)) / 4U;
        const int occlusion = static_cast<int>(opaqueA) + static_cast<int>(opaqueB)
            + static_cast<int>(opaqueDiagonal);
        const float ambient = 1.0f - static_cast<float>(occlusion) * 0.2f;
        const uint32_t shade = static_cast<uint32_t>(directionShade[face] * ambient * 255.0f);
        result[corner] = shade | blockLight << 8U | skyLight << 12U;
    }
    return result;
}

static uint32_t plantLighting(const Chunk& chunk, const int x, const int y, const int z) {
    return 255U | chunk.getLightData(x, y, z) << 8;
}

static bool isStairBlock(const Block block) {
    return BlockUtil::isStair(block);
}

static bool isSlabBlock(const Block block) {
    switch (blockId(block)) {
        case blockId(BLOCK_STONE_SLAB): case blockId(BLOCK_WOODEN_SLAB):
        case blockId(BLOCK_STONE_SLAB2): return true;
        default: return false;
    }
}

static bool isPaneBlock(const Block block) {
    return BlockUtil::isPane(block);
}

static bool isDoorBlock(const Block block) {
    switch (blockId(block)) {
        case blockId(BLOCK_WOODEN_DOOR): case blockId(BLOCK_IRON_DOOR):
        case blockId(BLOCK_SPRUCE_DOOR): case blockId(BLOCK_BIRCH_DOOR):
        case blockId(BLOCK_JUNGLE_DOOR): case blockId(BLOCK_ACACIA_DOOR):
        case blockId(BLOCK_DARK_OAK_DOOR): return true;
        default: return false;
    }
}

enum class FluidModel : uint8_t { none, water, lava };

static FluidModel fluidModel(const Block block) {
    switch (blockId(block)) {
        case blockId(BLOCK_WATER): case blockId(BLOCK_FLOWING_WATER): return FluidModel::water;
        case blockId(BLOCK_LAVA): case blockId(BLOCK_FLOWING_LAVA): return FluidModel::lava;
        default: return FluidModel::none;
    }
}

static bool isSkippedModel(const Block block) {
    switch (blockId(block)) {
        case blockId(BLOCK_AIR): case blockId(BLOCK_BARRIER):
        case blockId(BLOCK_STRUCTURE_VOID): return true;
        default: return false;
    }
}

enum class SpecialModelFamily : uint8_t {
    none,
    fence,
    pressurePlate,
    button,
    gate,
    sign
};

static SpecialModelFamily specialModelFamily(const Block block) {
    switch (blockId(block)) {
        case blockId(BLOCK_FENCE):
        case blockId(BLOCK_NETHER_BRICK_FENCE):
        case blockId(BLOCK_SPRUCE_FENCE):
        case blockId(BLOCK_BIRCH_FENCE):
        case blockId(BLOCK_JUNGLE_FENCE):
        case blockId(BLOCK_DARK_OAK_FENCE):
        case blockId(BLOCK_ACACIA_FENCE):
            return SpecialModelFamily::fence;
        case blockId(BLOCK_STONE_PRESSURE_PLATE):
        case blockId(BLOCK_WOODEN_PRESSURE_PLATE):
        case blockId(BLOCK_LIGHT_WEIGHTED_PRESSURE_PLATE):
        case blockId(BLOCK_HEAVY_WEIGHTED_PRESSURE_PLATE):
            return SpecialModelFamily::pressurePlate;
        case blockId(BLOCK_STONE_BUTTON):
        case blockId(BLOCK_WOODEN_BUTTON):
            return SpecialModelFamily::button;
        case blockId(BLOCK_FENCE_GATE):
        case blockId(BLOCK_SPRUCE_FENCE_GATE):
        case blockId(BLOCK_BIRCH_FENCE_GATE):
        case blockId(BLOCK_JUNGLE_FENCE_GATE):
        case blockId(BLOCK_DARK_OAK_FENCE_GATE):
        case blockId(BLOCK_ACACIA_FENCE_GATE):
            return SpecialModelFamily::gate;
        case blockId(BLOCK_STANDING_SIGN):
        case blockId(BLOCK_WALL_SIGN):
            return SpecialModelFamily::sign;
        default:
            return SpecialModelFamily::none;
    }
}

static bool usesCuboidModel(const Block block, const SpecialModelFamily family) {
    if (family != SpecialModelFamily::none || isStairBlock(block)
        || isSlabBlock(block) || isPaneBlock(block) || isDoorBlock(block)) return true;
    switch (blockId(block)) {
        case blockId(BLOCK_ANVIL): case blockId(BLOCK_COBBLESTONE_WALL):
        case blockId(BLOCK_BED): case blockId(BLOCK_END_PORTAL_FRAME):
        case blockId(BLOCK_TRAPDOOR): case blockId(BLOCK_IRON_TRAPDOOR):
        case blockId(BLOCK_DAYLIGHT_DETECTOR): case blockId(BLOCK_DAYLIGHT_DETECTOR_INVERTED):
        case blockId(BLOCK_SOUL_SAND): case blockId(BLOCK_SLIME):
        case blockId(BLOCK_PISTON): case blockId(BLOCK_STICKY_PISTON):
        case blockId(BLOCK_PISTON_HEAD):
        case blockId(BLOCK_ENCHANTING_TABLE): case blockId(BLOCK_FLOWER_POT):
        case blockId(BLOCK_CARPET): case blockId(BLOCK_CAULDRON): return true;
        default: return false;
    }
}

void Chunk::generateMesh(const BlockAtlasData &blockAtlas, const uint32_t sectionMask) const {
    ZoneScopedN("Chunk mesh and lighting");
    loadingMeshes.fetch_add(1, std::memory_order_relaxed);

    const auto westChunk = world->getLoadedChunk(chunkX - 1, chunkZ);
    const auto eastChunk = world->getLoadedChunk(chunkX + 1, chunkZ);
    const auto northChunk = world->getLoadedChunk(chunkX, chunkZ - 1);
    const auto southChunk = world->getLoadedChunk(chunkX, chunkZ + 1);
    const uint16_t grassSideOverlay = getNamedTileIndex(blockAtlas.second, "grass_side_overlay");
    const uint16_t grassSideSnowed = getNamedTileIndex(blockAtlas.second, "grass_side_snowed");
    const uint16_t waterStill = getNamedTileIndex(blockAtlas.second, "water_still");
    const uint16_t waterFlow = getNamedTileIndex(blockAtlas.second, "water_flow");
    const uint16_t lavaStill = getNamedTileIndex(blockAtlas.second, "lava_still");
    const uint16_t lavaFlow = getNamedTileIndex(blockAtlas.second, "lava_flow");

    const auto isNeighborOpaque = [&](const int x, const int y, const int z) -> uint8_t {
        if (y < 0 || y >= WORLD_HEIGHT) return 0;

        const Chunk* neighbor = nullptr;
        int localX = x;
        int localZ = z;

        if (x < 0) {
            neighbor = westChunk.get();
            localX = CHUNK_SIZE - 1;
        } else if (x >= CHUNK_SIZE) {
            neighbor = eastChunk.get();
            localX = 0;
        } else if (z < 0) {
            neighbor = northChunk.get();
            localZ = CHUNK_SIZE - 1;
        } else if (z >= CHUNK_SIZE) {
            neighbor = southChunk.get();
            localZ = 0;
        } else {
            return BlockUtil::isFullBlock(getBlock(x, y, z)) ? 1 : 0;
        }

        if (neighbor == nullptr || !neighbor->dataReady.load(std::memory_order_acquire)) return 2;
        return BlockUtil::isFullBlock(neighbor->getBlock(localX, y, localZ)) ? 1 : 0;
    };

    const auto neighborBlock = [&](const int x, const int y, const int z) -> Block {
        if (y < 0 || y >= WORLD_HEIGHT) return BLOCK_AIR;
        if (x >= 0 && x < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE) return getBlock(x, y, z);

        const Chunk* neighbor = nullptr;
        int localX = x;
        int localZ = z;
        if (x < 0) { neighbor = westChunk.get(); localX = CHUNK_SIZE - 1; }
        else if (x >= CHUNK_SIZE) { neighbor = eastChunk.get(); localX = 0; }
        else if (z < 0) { neighbor = northChunk.get(); localZ = CHUNK_SIZE - 1; }
        else if (z >= CHUNK_SIZE) { neighbor = southChunk.get(); localZ = 0; }
        return neighbor == nullptr || !neighbor->dataReady.load(std::memory_order_acquire)
            ? BLOCK_AIR : neighbor->getBlock(localX, y, localZ);
    };

    for (int section = 0; section < WORLD_HEIGHT / CHUNK_SIZE; ++section) {
    if ((sectionMask >> section & 1U) == 0) continue;
    meshData.vertices.clear();
    meshData.translucentVertices.clear();
    meshData.skullBlocks.clear();
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int y = section * CHUNK_SIZE; y < (section + 1) * CHUNK_SIZE; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {

                const Block block = getBlock(x, y, z);
                glm::ivec3 pos{x, y, z};
                // skulls and the block entity models are drawn by Renderer::renderBlockEntityModels; the mesh
                // only records where they are (the list keeps its old name)
                if (blockId(block) == blockId(BLOCK_SKULL) || BlockUtil::isBlockEntityModel(block)) {
                    meshData.skullBlocks.push_back(pos);
                    continue;
                }
                if (BlockUtil::hasBlockEntityExtras(block)) meshData.skullBlocks.push_back(pos);

                if (!BlockUtil::isOpaque(block)) {
                    if (x == 0 || x == CHUNK_SIZE-1 || z == 0 || z == CHUNK_SIZE-1) {
                        // lazy-loaded
                        if (const auto it = pendingFaces.find(pos); it != pendingFaces.end()) {
                            for (const auto &pf : it->second) {
                                const uint16_t faceData = static_cast<uint16_t>(pf);
                                const uint16_t face = faceData & 0x7;
                                const uint8_t height = faceData >> 3 & 0x7;
                                const uint16_t tileIndex = faceData >> 6;
                                const uint32_t tint = static_cast<uint32_t>(pf >> 16);

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
                                pushQuad(meshData.vertices, face, realPos.x, realPos.y, realPos.z, tileIndex, height, tint,
                                    faceLighting(*this, realPos.x, realPos.y, realPos.z, face));
                            }
                            pendingFaces.erase(it);
                        }
                    }
                    if (block == BLOCK_AIR) continue;
                }

                if (isSkippedModel(block)) continue;

                const FluidModel fluid = fluidModel(block);
                const bool isWater = fluid == FluidModel::water;
                const bool isLava = fluid == FluidModel::lava;
                if (isWater || isLava) {
                    const auto isSameFluid = [&](const Block candidate) {
                        if (isWater) {
                            return blockId(candidate) == blockId(BLOCK_WATER)
                                || blockId(candidate) == blockId(BLOCK_FLOWING_WATER);
                        }
                        return blockId(candidate) == blockId(BLOCK_LAVA)
                            || blockId(candidate) == blockId(BLOCK_FLOWING_LAVA);
                    };
                    const bool covered = isSameFluid(neighborBlock(x, y + 1, z));
                    const auto fluidHeight = [&](const int sampleX, const int sampleZ) {
                        int samples = 0;
                        float total = 0.0f;
                        for (int corner = 0; corner < 4; ++corner) {
                            const int blockX = sampleX - (corner & 1);
                            const int blockZ = sampleZ - (corner >> 1 & 1);
                            if (isSameFluid(neighborBlock(blockX, y + 1, blockZ))) return 1.0f;
                            const Block sample = neighborBlock(blockX, y, blockZ);
                            if (!isSameFluid(sample)) {
                                if (!BlockUtil::isSolidMaterial(sample)) {
                                    total += 1.0f;
                                    ++samples;
                                }
                                continue;
                            }
                            const int level = blockMeta(sample);
                            const float percent = static_cast<float>(std::min(level, 7) + 1) / 9.0f;
                            if (level == 0 || level >= 8) {
                                total += percent * 10.0f;
                                samples += 10;
                            }
                            total += percent;
                            ++samples;
                        }
                        return samples == 0 ? 0.0f : 1.0f - total / static_cast<float>(samples);
                    };
                    const auto encodedHeight = [&](const float height) {
                        const float insetHeight = covered ? 1.0f : height - 0.001f;
                        return static_cast<uint8_t>(std::clamp(std::round(insetHeight * 255.0f), 0.0f, 255.0f));
                    };
                    const uint8_t h00 = encodedHeight(fluidHeight(x, z));
                    const uint8_t h01 = encodedHeight(fluidHeight(x, z + 1));
                    const uint8_t h11 = encodedHeight(fluidHeight(x + 1, z + 1));
                    const uint8_t h10 = encodedHeight(fluidHeight(x + 1, z));
                    const auto effectiveDecay = [&](const int sampleX, const int sampleY, const int sampleZ) {
                        const Block sample = neighborBlock(sampleX, sampleY, sampleZ);
                        if (!isSameFluid(sample)) return -1;
                        const int level = blockMeta(sample);
                        return level >= 8 ? 0 : level;
                    };
                    const int decay = effectiveDecay(x, y, z);
                    float flowX = 0.0f;
                    float flowZ = 0.0f;
                    static constexpr int horizontal[4][2] = {{0,-1},{0,1},{-1,0},{1,0}};
                    for (const auto& direction : horizontal) {
                        const int sampleX = x + direction[0];
                        const int sampleZ = z + direction[1];
                        int neighborDecay = effectiveDecay(sampleX, y, sampleZ);
                        int difference = 0;
                        if (neighborDecay < 0) {
                            if (BlockUtil::blocksMovement(neighborBlock(sampleX, y, sampleZ))) continue;
                            neighborDecay = effectiveDecay(sampleX, y - 1, sampleZ);
                            if (neighborDecay < 0) continue;
                            difference = neighborDecay - (decay - 8);
                        } else {
                            difference = neighborDecay - decay;
                        }
                        flowX += static_cast<float>(direction[0] * difference);
                        flowZ += static_cast<float>(direction[1] * difference);
                    }
                    const bool flowing = flowX != 0.0f || flowZ != 0.0f;
                    const uint16_t stillTile = isWater ? waterStill : lavaStill;
                    const uint16_t flowTile = isWater ? waterFlow : lavaFlow;
                    std::array<std::array<uint8_t, 2>, 4> topUvs{{{0,0},{0,16},{16,16},{16,0}}};
                    if (flowing) {
                        const float angle = std::atan2(flowZ, flowX) - static_cast<float>(M_PI) * 0.5f;
                        const float sine = std::sin(angle) * 0.25f;
                        const float cosine = std::cos(angle) * 0.25f;
                        const auto uv = [](const float value) {
                            return static_cast<uint8_t>(std::clamp(std::round(8.0f + (value - 8.0f) * 2.0f),
                                0.0f, 16.0f));
                        };
                        topUvs = {{{uv(8.0f + (-cosine - sine) * 16.0f), uv(8.0f + (-cosine + sine) * 16.0f)},
                                   {uv(8.0f + (-cosine + sine) * 16.0f), uv(8.0f + ( cosine + sine) * 16.0f)},
                                   {uv(8.0f + ( cosine + sine) * 16.0f), uv(8.0f + ( cosine - sine) * 16.0f)},
                                   {uv(8.0f + ( cosine - sine) * 16.0f), uv(8.0f + (-cosine - sine) * 16.0f)}}};
                    }
                    for (int f = 0; f < 6; ++f) {
                        int nx = x, ny = y, nz = z;
                        switch (f) {
                            case 0: ++nz; break;
                            case 1: --nz; break;
                            case 2: --nx; break;
                            case 3: ++nx; break;
                            case 4: ++ny; break;
                            case 5: --ny; break;
                            default: break;
                        }
                        if (const Block neighbor = neighborBlock(nx, ny, nz); isSameFluid(neighbor) || BlockUtil::isOpaque(neighbor)) continue;
                        const auto lighting = smoothFaceLighting(*this, x, y, z, f);
                        switch (f) {
                            case 0:
                                pushFluidFace(meshData.translucentVertices, x, y, z, f, flowTile,
                                    {{{0,0,16},{16,0,16},{16,h11,16},{0,h01,16}}},
                                    {{{0,16},{16,16},{16,static_cast<uint8_t>((255-h11)*16/255)},{0,static_cast<uint8_t>((255-h01)*16/255)}}}, lighting);
                                break;
                            case 1:
                                pushFluidFace(meshData.translucentVertices, x, y, z, f, flowTile,
                                    {{{16,0,0},{0,0,0},{0,h00,0},{16,h10,0}}},
                                    {{{0,16},{16,16},{16,static_cast<uint8_t>((255-h00)*16/255)},{0,static_cast<uint8_t>((255-h10)*16/255)}}}, lighting);
                                break;
                            case 2:
                                pushFluidFace(meshData.translucentVertices, x, y, z, f, flowTile,
                                    {{{0,0,0},{0,0,16},{0,h01,16},{0,h00,0}}},
                                    {{{0,16},{16,16},{16,static_cast<uint8_t>((255-h01)*16/255)},{0,static_cast<uint8_t>((255-h00)*16/255)}}}, lighting);
                                break;
                            case 3:
                                pushFluidFace(meshData.translucentVertices, x, y, z, f, flowTile,
                                    {{{16,0,16},{16,0,0},{16,h10,0},{16,h11,16}}},
                                    {{{0,16},{16,16},{16,static_cast<uint8_t>((255-h10)*16/255)},{0,static_cast<uint8_t>((255-h11)*16/255)}}}, lighting);
                                break;
                            case 4:
                                pushFluidFace(meshData.translucentVertices, x, y, z, f,
                                    flowing ? flowTile : stillTile,
                                    {{{0,h00,0},{0,h01,16},{16,h11,16},{16,h10,0}}},
                                    topUvs, lighting);
                                break;
                            case 5:
                                pushFluidFace(meshData.translucentVertices, x, y, z, f, stillTile,
                                    {{{0,0,0},{16,0,0},{16,0,16},{0,0,16}}},
                                    {{{0,0},{16,0},{16,16},{0,16}}}, lighting);
                                break;
                            default: break;
                        }
                    }
                    continue;
                }

                const uint16_t modelId = blockId(block);
                if (const SpecialModelFamily modelFamily = specialModelFamily(block); usesCuboidModel(block, modelFamily)) {
                    std::array<uint16_t, 6> tiles{};
                    std::array<std::array<uint32_t, 4>, 6> lighting{};
                    std::array<bool, 6> outerFaces{};
                    for (int face = 0; face < 6; ++face) {
                        tiles[face] = getTileIndex(blockAtlas.second, block, face);
                        lighting[face] = smoothFaceLighting(*this, x, y, z, face);
                        int nx = x, ny = y, nz = z;
                        switch (face) {
                            case 0: ++nz; break;
                            case 1: --nz; break;
                            case 2: --nx; break;
                            case 3: ++nx; break;
                            case 4: ++ny; break;
                            case 5: --ny; break;
                            default: break;
                        }
                        outerFaces[face] = isNeighborOpaque(nx, ny, nz) != 1;
                        // adjacent translucent blocks of the same type share one volume and emitting both boundary faces blends the texture twice
                        if (modelId == blockId(BLOCK_SLIME)
                            && blockId(neighborBlock(nx, ny, nz)) == blockId(BLOCK_SLIME)) {
                            outerFaces[face] = false;
                        }
                    }
                    switch (modelId) {
                        case blockId(BLOCK_PISTON): case blockId(BLOCK_STICKY_PISTON):
                        case blockId(BLOCK_PISTON_HEAD):
                            tiles.fill(getNamedTileIndex(blockAtlas.second, "piston_side"));
                            break;
                        case blockId(BLOCK_IRON_BARS):
                            tiles.fill(getNamedTileIndex(blockAtlas.second, "iron_bars"));
                            break;
                        case blockId(BLOCK_GLASS_PANE):
                            for (int face = 0; face < 4; ++face)
                                tiles[face] = getNamedTileIndex(blockAtlas.second, "glass");
                            tiles[4] = tiles[5] = getNamedTileIndex(blockAtlas.second, "glass_pane_top");
                            break;
                        case blockId(BLOCK_STAINED_GLASS_PANE): {
                            const std::string suffix = BlockUtil::dyeColorNames[blockMeta(block) & 15];
                            for (int face = 0; face < 4; ++face)
                                tiles[face] = getNamedTileIndex(blockAtlas.second, "glass_" + suffix);
                            tiles[4] = tiles[5] = getNamedTileIndex(blockAtlas.second, "glass_pane_top_" + suffix);
                            break;
                        }
                        default: break;
                    }
                    // blocks with no entry in the block texture table get their atlas textures by name, otherwise they would fall back to dirt
                    const auto named = [&](const char* name) { return getNamedTileIndex(blockAtlas.second, name); };
                    switch (modelId) {
                        case blockId(BLOCK_FENCE): tiles.fill(named("planks_oak")); break;
                        case blockId(BLOCK_NETHER_BRICK_FENCE): tiles.fill(named("nether_brick")); break;
                        case blockId(BLOCK_SPRUCE_FENCE): tiles.fill(named("planks_spruce")); break;
                        case blockId(BLOCK_BIRCH_FENCE): tiles.fill(named("planks_birch")); break;
                        case blockId(BLOCK_JUNGLE_FENCE): tiles.fill(named("planks_jungle")); break;
                        case blockId(BLOCK_DARK_OAK_FENCE): tiles.fill(named("planks_big_oak")); break;
                        case blockId(BLOCK_ACACIA_FENCE): tiles.fill(named("planks_acacia")); break;
                        case blockId(BLOCK_COBBLESTONE_WALL):
                            tiles.fill(named((blockMeta(block) & 1) != 0 ? "cobblestone_mossy" : "cobblestone"));
                            break;
                        case blockId(BLOCK_STONE_PRESSURE_PLATE): tiles.fill(named("stone")); break;
                        case blockId(BLOCK_WOODEN_PRESSURE_PLATE): tiles.fill(named("planks_oak")); break;
                        case blockId(BLOCK_LIGHT_WEIGHTED_PRESSURE_PLATE): tiles.fill(named("gold_block")); break;
                        case blockId(BLOCK_HEAVY_WEIGHTED_PRESSURE_PLATE): tiles.fill(named("iron_block")); break;
                        case blockId(BLOCK_END_PORTAL_FRAME):
                            tiles.fill(named("endframe_side"));
                            tiles[4] = named("endframe_top");
                            tiles[5] = named("end_stone");
                            break;
                        case blockId(BLOCK_TRAPDOOR): tiles.fill(named("trapdoor")); break;
                        case blockId(BLOCK_IRON_TRAPDOOR): tiles.fill(named("iron_trapdoor")); break;
                        case blockId(BLOCK_SOUL_SAND): tiles.fill(named("soul_sand")); break;
                        case blockId(BLOCK_SLIME): tiles.fill(named("slime")); break;
                        case blockId(BLOCK_STONE_BUTTON): tiles.fill(named("stone")); break;
                        case blockId(BLOCK_WOODEN_BUTTON): tiles.fill(named("planks_oak")); break;
                        case blockId(BLOCK_ENCHANTING_TABLE):
                            tiles.fill(named("enchanting_table_side"));
                            tiles[4] = named("enchanting_table_top");
                            tiles[5] = named("enchanting_table_bottom");
                            break;
                        case blockId(BLOCK_FLOWER_POT): tiles.fill(named("flower_pot")); break;
                        case blockId(BLOCK_CAULDRON):
                            tiles.fill(named("cauldron_side"));
                            tiles[4] = named("cauldron_top");
                            tiles[5] = named("cauldron_bottom");
                            break;
                        case blockId(BLOCK_STANDING_SIGN): case blockId(BLOCK_WALL_SIGN): tiles.fill(named("planks_oak")); break;
                        case blockId(BLOCK_FENCE_GATE): tiles.fill(named("planks_oak")); break;
                        case blockId(BLOCK_SPRUCE_FENCE_GATE): tiles.fill(named("planks_spruce")); break;
                        case blockId(BLOCK_BIRCH_FENCE_GATE): tiles.fill(named("planks_birch")); break;
                        case blockId(BLOCK_JUNGLE_FENCE_GATE): tiles.fill(named("planks_jungle")); break;
                        case blockId(BLOCK_DARK_OAK_FENCE_GATE): tiles.fill(named("planks_big_oak")); break;
                        case blockId(BLOCK_ACACIA_FENCE_GATE): tiles.fill(named("planks_acacia")); break;
                        case blockId(BLOCK_DAYLIGHT_DETECTOR): case blockId(BLOCK_DAYLIGHT_DETECTOR_INVERTED):
                            tiles.fill(named("daylight_detector_side"));
                            tiles[4] = named(modelId == blockId(BLOCK_DAYLIGHT_DETECTOR)
                                ? "daylight_detector_top" : "daylight_detector_inverted_top");
                            break;
                        case blockId(BLOCK_BED): {
                            // meta & 3 is the direction from the foot toward the head (south, west, north, east)
                            const bool head = (blockMeta(block) & 8) != 0;
                            static constexpr int towardHead[4]{0, 2, 1, 3}; // geometry face of south/west/north/east
                            static constexpr int awayFromHead[4]{1, 3, 0, 2};
                            const int end = head ? towardHead[blockMeta(block) & 3] : awayFromHead[blockMeta(block) & 3];
                            tiles.fill(named(head ? "bed_head_side" : "bed_feet_side"));
                            tiles[end] = named(head ? "bed_head_end" : "bed_feet_end");
                            tiles[4] = named(head ? "bed_head_top" : "bed_feet_top");
                            tiles[5] = named("planks_oak");
                            break;
                        }
                        default: break;
                    }
                    // trapdoors have see-through pixels, so they use the alpha-tested layer
                    bool trapdoor = false;
                    switch (modelId) {
                        case blockId(BLOCK_TRAPDOOR): case blockId(BLOCK_IRON_TRAPDOOR):
                            trapdoor = true;
                            break;
                        default: break;
                    }
                    const bool cutoutModel = trapdoor || isDoorBlock(block);
                    const uint32_t tint = cutoutModel ? (1U << 31U)
                        : tintData(block, 0, biomes[z * CHUNK_SIZE + x]);
                    const auto box = [&](const std::array<uint8_t, 3>& minimum,
                                         const std::array<uint8_t, 3>& maximum,
                                         const std::array<bool, 6>& faces,
                                         const uint8_t topUvTurns = 0,
                                         const bool flipSideU = false,
                                         const bool forceOpaque = false,
                                         const uint8_t modelFacing = 255,
                                         const uint16_t faceUvTurns = 0) {
                        bool translucentModel = false;
                        switch (modelId) {
                            case blockId(BLOCK_STAINED_GLASS_PANE): case blockId(BLOCK_SLIME):
                                translucentModel = !forceOpaque;
                                break;
                            default: break;
                        }
                        auto& vertices = translucentModel
                            ? meshData.translucentVertices : meshData.vertices;
                        std::array<std::array<uint32_t, 4>, 6> boxLighting = lighting;
                        switch (modelFamily) {
                        case SpecialModelFamily::fence: case SpecialModelFamily::gate: {
                            static constexpr float directionShade[6]{0.8f, 0.8f, 0.6f, 0.6f, 1.0f, 0.5f};
                            for (int face = 0; face < 6; ++face) {
                                const uint32_t own = static_cast<uint32_t>(directionShade[face] * 255.0f)
                                    | getLightData(x, y, z) << 8U;
                                boxLighting[face].fill(own);
                            }
                            break;
                        }
                        default: break;
                        }
                        pushModelCuboid(vertices, x, y, z, tiles, tint, faces,
                            minimum, maximum, boxLighting, topUvTurns, flipSideU, modelFacing, faceUvTurns);
                    };

                    if (isSlabBlock(block)) {
                        if ((blockMeta(block) & 8) != 0) box({0, 8, 0}, {16, 16, 16}, outerFaces);
                        else box({0, 0, 0}, {16, 8, 16}, outerFaces);
                        continue;
                    }

                    if (isStairBlock(block)) {
                        // a half-height slab plus the upper quadrants chosen by the stair shape
                        const uint8_t upper = BlockUtil::stairUpperQuadrants(block, [&](const int dx, const int dz) {
                            return neighborBlock(x + dx, y, z + dz);
                        });
                        const bool upsideDown = (blockMeta(block) & 4) != 0;
                        // exists(high, q): is there a box in the upper (y 8-16) or lower (y 0-8) layer
                        const auto exists = [&](const bool high, const int quadrant) {
                            return high == upsideDown ? true : (upper >> quadrant & 1) != 0;
                        };
                        for (int quadrant = 0; quadrant < 4; ++quadrant) {
                            const int qx = quadrant & 1, qz = quadrant >> 1;
                            for (const bool high : {false, true}) {
                                if (!exists(high, quadrant)) continue;
                                std::array<bool, 6> faces{};
                                faces[0] = qz == 1 ? outerFaces[0] : !exists(high, qx + 2);
                                faces[1] = qz == 0 ? outerFaces[1] : !exists(high, qx);
                                faces[2] = qx == 0 ? outerFaces[2] : !exists(high, qz * 2);
                                faces[3] = qx == 1 ? outerFaces[3] : !exists(high, qz * 2 + 1);
                                faces[4] = high ? outerFaces[4] : !exists(true, quadrant);
                                faces[5] = high ? !exists(false, quadrant) : outerFaces[5];
                                // faces inside the block take the block's own light; only faces on its boundary use the smooth light of the neighboring cell
                                const bool boundary[6]{qz == 1, qz == 0, qx == 0, qx == 1, high, !high};
                                static constexpr float directionShade[6]{0.8f, 0.8f, 0.6f, 0.6f, 1.0f, 0.5f};
                                std::array<std::array<uint32_t, 4>, 6> boxLighting = lighting;
                                const auto averageLight = [](const uint32_t first,
                                                             const uint32_t second) {
                                    const uint32_t shade = ((first & 255U) + (second & 255U)) / 2U;
                                    const uint32_t block = (((first >> 8U) & 15U)
                                        + ((second >> 8U) & 15U)) / 2U;
                                    const uint32_t sky = (((first >> 12U) & 15U)
                                        + ((second >> 12U) & 15U)) / 2U;
                                    return shade | block << 8U | sky << 12U;
                                };
                                for (int face = 0; face < 4; ++face) {
                                    const uint32_t firstMidpoint = averageLight(
                                        lighting[face][0], lighting[face][3]);
                                    const uint32_t secondMidpoint = averageLight(
                                        lighting[face][1], lighting[face][2]);
                                    if (high) {
                                        boxLighting[face][0] = firstMidpoint;
                                        boxLighting[face][1] = secondMidpoint;
                                    } else {
                                        boxLighting[face][2] = secondMidpoint;
                                        boxLighting[face][3] = firstMidpoint;
                                    }
                                }
                                for (int face = 0; face < 6; ++face) {
                                    if (boundary[face]) continue;
                                    const uint32_t own = static_cast<uint32_t>(directionShade[face] * 255.0f)
                                        | getLightData(x, y, z) << 8;
                                    boxLighting[face].fill(own);
                                }
                                pushModelCuboid(meshData.vertices, x, y, z, tiles, tint, faces,
                                    {static_cast<uint8_t>(qx * 8), static_cast<uint8_t>(high ? 8 : 0), static_cast<uint8_t>(qz * 8)},
                                    {static_cast<uint8_t>(qx * 8 + 8), static_cast<uint8_t>(high ? 16 : 8), static_cast<uint8_t>(qz * 8 + 8)},
                                    boxLighting);
                            }
                        }
                        continue;
                    }

                    if (isPaneBlock(block)) {
                        const auto connects = [&](const Block neighbor) {
                            return BlockUtil::canPaneConnectTo(neighbor);
                        };
                        bool north = connects(neighborBlock(x, y, z - 1));
                        bool south = connects(neighborBlock(x, y, z + 1));
                        bool west = connects(neighborBlock(x - 1, y, z));
                        bool east = connects(neighborBlock(x + 1, y, z));
                        if (!north && !south && !west && !east) north = south = west = east = true;
                        static constexpr std::array allFaces{true, true, true, true, true, true};
                        box({7, 0, 7}, {9, 16, 9}, allFaces);
                        if (north) box({7, 0, 0}, {9, 16, 8}, allFaces);
                        if (south) box({7, 0, 8}, {9, 16, 16}, allFaces);
                        if (west) box({0, 0, 7}, {8, 16, 9}, allFaces);
                        if (east) box({8, 0, 7}, {16, 16, 9}, allFaces);
                        continue;
                    }

                    static constexpr std::array allFaces{true, true, true, true, true, true};
                    if (modelId == blockId(BLOCK_PISTON) || modelId == blockId(BLOCK_STICKY_PISTON)
                        || modelId == blockId(BLOCK_PISTON_HEAD)) {
                        const uint8_t facing = blockMeta(block) & 7;
                        const auto faceForFacing = [](const uint8_t direction) -> uint8_t {
                            switch (direction) {
                                case 0: return 5;
                                case 1: return 4;
                                case 2: return 1;
                                case 3: return 0;
                                case 4: return 2;
                                case 5: return 3;
                                default: return 1;
                            }
                        };
                        const auto oppositeFace = [](const uint8_t face) -> uint8_t {
                            switch (face) {
                                case 0: return 1;
                                case 1: return 0;
                                case 2: return 3;
                                case 3: return 2;
                                case 4: return 5;
                                default: return 4;
                            }
                        };
                        const auto orient = [&](std::array<uint8_t, 3> low,
                                                std::array<uint8_t, 3> high) {
                            switch (facing) {
                                case 0: return std::pair{std::array<uint8_t, 3>{low[0], low[2], low[1]},
                                    std::array<uint8_t, 3>{high[0], high[2], high[1]}};
                                case 1: return std::pair{std::array<uint8_t, 3>{low[0], static_cast<uint8_t>(16 - high[2]), low[1]},
                                    std::array<uint8_t, 3>{high[0], static_cast<uint8_t>(16 - low[2]), high[1]}};
                                case 3: return std::pair{std::array<uint8_t, 3>{static_cast<uint8_t>(16 - high[0]), low[1], static_cast<uint8_t>(16 - high[2])},
                                    std::array<uint8_t, 3>{static_cast<uint8_t>(16 - low[0]), high[1], static_cast<uint8_t>(16 - low[2])}};
                                case 4: return std::pair{std::array<uint8_t, 3>{low[2], low[1], static_cast<uint8_t>(16 - high[0])},
                                    std::array<uint8_t, 3>{high[2], high[1], static_cast<uint8_t>(16 - low[0])}};
                                case 5: return std::pair{std::array<uint8_t, 3>{static_cast<uint8_t>(16 - high[2]), low[1], low[0]},
                                    std::array<uint8_t, 3>{static_cast<uint8_t>(16 - low[2]), high[1], high[0]}};
                                default: return std::pair{low, high};
                            }
                        };
                        const uint8_t front = faceForFacing(facing);
                        const uint8_t back = oppositeFace(front);
                        uint8_t uvFacing = facing;
                        uint16_t uvTurns = 0;
                        switch (facing) {
                            case 1:
                                uvFacing = 255;
                                break;
                            case 4:
                                uvFacing = 255;
                                uvTurns = static_cast<uint16_t>(1U << (0 * 2U) | 1U << (1 * 2U)
                                    | 2U << (2 * 2U) | 2U << (3 * 2U)
                                    | 1U << (4 * 2U) | 1U << (5 * 2U));
                                break;
                            default: break;
                        }
                        if (modelId == blockId(BLOCK_PISTON_HEAD)) {
                            tiles[front] = named((blockMeta(block) & 8) != 0
                                ? "piston_top_sticky" : "piston_top_normal");
                            tiles[back] = named("piston_top_normal");
                            const auto [plateLow, plateHigh] = orient({0, 0, 0}, {16, 16, 4});
                            box(plateLow, plateHigh, allFaces, 0, false, false, uvFacing, uvTurns);
                            std::array<bool, 6> rodFaces = allFaces;
                            rodFaces[front] = rodFaces[back] = false;
                            const auto [rodLow, rodHigh] = orient({6, 6, 4}, {10, 10, 16});
                            box(rodLow, rodHigh, rodFaces, 0, false, false, uvFacing, uvTurns);
                        } else {
                            tiles[front] = named(modelId == blockId(BLOCK_STICKY_PISTON)
                                ? "piston_top_sticky" : "piston_top_normal");
                            tiles[back] = named("piston_bottom");
                            if ((blockMeta(block) & 8) != 0) {
                                tiles[front] = named("piston_inner");
                                const auto [low, high] = orient({0, 0, 4}, {16, 16, 16});
                                box(low, high, allFaces, 0, false, false, uvFacing, uvTurns);
                            } else {
                                box({0, 0, 0}, {16, 16, 16}, outerFaces, 0, false, false, uvFacing, uvTurns);
                            }
                        }
                        continue;
                    }
                    if (trapdoor) {
                        // blockTrapDoor.setBounds: 3/16 thick, flat at the bottom or top, or upright on its wall when open
                        const uint8_t meta = blockMeta(block);
                        std::array<uint8_t, 3> low{0, 0, 0}, high{16, 3, 16};
                        if ((meta & 8) != 0) { low[1] = 13; high[1] = 16; }
                        if ((meta & 4) != 0) {
                            low = {0, 0, 0}; high = {16, 16, 16};
                            switch (meta & 3) {
                                case 0: low[2] = 13; break; // north
                                case 1: high[2] = 3; break;
                                case 2: low[0] = 13; break;
                                default: high[0] = 3; break;
                            }
                        }
                        box(low, high, allFaces);
                        continue;
                    }
                    switch (modelId) {
                        case blockId(BLOCK_DAYLIGHT_DETECTOR):
                        case blockId(BLOCK_DAYLIGHT_DETECTOR_INVERTED):
                            box({0, 0, 0}, {16, 6, 16}, allFaces);
                            continue;
                        case blockId(BLOCK_SOUL_SAND):
                            // drawn as a full cube, only its collision is lower
                            box({0, 0, 0}, {16, 16, 16}, outerFaces);
                            continue;
                        case blockId(BLOCK_SLIME):
                            box({0, 0, 0}, {16, 16, 16}, outerFaces);
                            box({3, 3, 3}, {13, 13, 13}, allFaces, 0, false, true);
                            continue;
                        case blockId(BLOCK_STONE_BUTTON): case blockId(BLOCK_WOODEN_BUTTON): {
                            const uint8_t meta = blockMeta(block);
                            const uint8_t depth = (meta & 8) != 0 ? 1 : 2;
                            switch (meta & 7) {
                                case 1: box({0, 6, 5}, {depth, 10, 11}, allFaces); break;
                                case 2: box({static_cast<uint8_t>(16 - depth), 6, 5}, {16, 10, 11}, allFaces); break;
                                case 3: box({5, 6, 0}, {11, 10, depth}, allFaces); break;
                                case 4: box({5, 6, static_cast<uint8_t>(16 - depth)}, {11, 10, 16}, allFaces); break;
                                default: break;
                            }
                            continue;
                        }
                        case blockId(BLOCK_ENCHANTING_TABLE):
                            box({0, 0, 0}, {16, 12, 16}, allFaces);
                            continue;
                        case blockId(BLOCK_FLOWER_POT):
                            box({5, 0, 5}, {6, 6, 11}, allFaces);
                            box({10, 0, 5}, {11, 6, 11}, allFaces);
                            box({6, 0, 5}, {10, 6, 6}, allFaces);
                            box({6, 0, 10}, {10, 6, 11}, allFaces);
                            tiles.fill(named("dirt"));
                            box({6, 0, 6}, {10, 4, 10}, allFaces);
                            continue;
                        case blockId(BLOCK_BED):
                            box({0, 0, 0}, {16, 9, 16}, allFaces,
                                static_cast<uint8_t>((4 - (blockMeta(block) & 3)) & 3), true);
                            continue;
                        case blockId(BLOCK_CARPET):
                            box({0, 0, 0}, {16, 1, 16}, outerFaces);
                            continue;
                        case blockId(BLOCK_CAULDRON):
                            tiles[4] = named("cauldron_inner");
                            box({0, 0, 0}, {16, 5, 16}, allFaces);
                            tiles[4] = named("cauldron_top");
                            box({0, 5, 0}, {2, 16, 16}, allFaces);
                            box({14, 5, 0}, {16, 16, 16}, allFaces);
                            box({2, 5, 0}, {14, 16, 2}, allFaces);
                            box({2, 5, 14}, {14, 16, 16}, allFaces);
                            continue;
                        case blockId(BLOCK_END_PORTAL_FRAME):
                            box({0, 0, 0}, {16, 13, 16}, allFaces);
                            if ((blockMeta(block) & 4) != 0) {
                                tiles.fill(getNamedTileIndex(blockAtlas.second, "endframe_eye"));
                                box({5, 13, 5}, {11, 16, 11}, allFaces);
                            }
                            continue;
                        default:
                            break;
                        }
                    switch (modelFamily) {
                    case SpecialModelFamily::sign: {
                        switch (modelId) {
                        case blockId(BLOCK_WALL_SIGN):
                            switch (blockMeta(block)) {
                                case 2: box({0, 5, 14}, {16, 13, 16}, allFaces); break;
                                case 3: box({0, 5, 0}, {16, 13, 2}, allFaces); break;
                                case 4: box({14, 5, 0}, {16, 13, 16}, allFaces); break;
                                default: box({0, 5, 0}, {2, 13, 16}, allFaces); break;
                            }
                            break;
                        default: {
                            const bool alongX = ((blockMeta(block) + 2) & 7) < 4;
                            if (alongX) box({0, 8, 7}, {16, 16, 9}, allFaces);
                            else box({7, 8, 0}, {9, 16, 16}, allFaces);
                            box({7, 0, 7}, {9, 8, 9}, allFaces);
                            break;
                        }
                        }
                        continue;
                    }
                    case SpecialModelFamily::gate: {
                        const bool rotated = (blockMeta(block) & 1) != 0;
                        const bool open = (blockMeta(block) & 4) != 0;
                        const auto gateBox = [&](std::array<uint8_t, 3> low, std::array<uint8_t, 3> high) {
                            if (rotated) { std::swap(low[0], low[2]); std::swap(high[0], high[2]); }
                            box(low, high, allFaces);
                        };
                        gateBox({0, 5, 7}, {2, 16, 9});
                        gateBox({14, 5, 7}, {16, 16, 9});
                        if (!open) {
                            gateBox({6, 6, 7}, {10, 15, 9});
                            gateBox({2, 6, 7}, {6, 9, 9}); gateBox({2, 12, 7}, {6, 15, 9});
                            gateBox({10, 6, 7}, {14, 9, 9}); gateBox({10, 12, 7}, {14, 15, 9});
                        } else {
                            gateBox({0, 6, 13}, {2, 15, 15});
                            gateBox({14, 6, 13}, {16, 15, 15});
                            gateBox({0, 6, 9}, {2, 9, 13}); gateBox({0, 12, 9}, {2, 15, 13});
                            gateBox({14, 6, 9}, {16, 9, 13}); gateBox({14, 12, 9}, {16, 15, 13});
                        }
                        continue;
                    }
                    case SpecialModelFamily::pressurePlate:
                        box({1, 0, 1}, {15, 1, 15}, allFaces);
                        continue;
                    default:
                        break;
                    }

                    switch (modelId) {
                    case blockId(BLOCK_WOODEN_DOOR): case blockId(BLOCK_IRON_DOOR):
                    case blockId(BLOCK_SPRUCE_DOOR): case blockId(BLOCK_BIRCH_DOOR):
                    case blockId(BLOCK_JUNGLE_DOOR): case blockId(BLOCK_ACACIA_DOOR):
                    case blockId(BLOCK_DARK_OAK_DOOR): {
                        const bool upperHalf = (blockMeta(block) & 8) != 0;
                        const Block lower = upperHalf ? neighborBlock(x, y - 1, z) : block;
                        const Block upper = upperHalf ? block : neighborBlock(x, y + 1, z);
                        const uint8_t plane = BlockUtil::doorPlane(blockMeta(lower), blockMeta(upper));
                        const char* material = "door_wood";
                        switch (modelId) {
                            case blockId(BLOCK_IRON_DOOR): material = "door_iron"; break;
                            case blockId(BLOCK_SPRUCE_DOOR): material = "door_spruce"; break;
                            case blockId(BLOCK_BIRCH_DOOR): material = "door_birch"; break;
                            case blockId(BLOCK_JUNGLE_DOOR): material = "door_jungle"; break;
                            case blockId(BLOCK_ACACIA_DOOR): material = "door_acacia"; break;
                            case blockId(BLOCK_DARK_OAK_DOOR): material = "door_dark_oak"; break;
                            default: break;
                        }
                        tiles.fill(named((std::string(material) + (upperHalf ? "_upper" : "_lower")).c_str()));
                        switch (plane) {
                            case 0: box({0, 0, 0}, {16, 16, 3}, allFaces); break;
                            case 1: box({13, 0, 0}, {16, 16, 16}, allFaces); break;
                            case 2: box({0, 0, 13}, {16, 16, 16}, allFaces); break;
                            case 3: box({0, 0, 0}, {3, 16, 16}, allFaces); break;
                            default: break;
                        }
                        continue;
                    }
                    default: break;
                    }

                    switch (modelFamily) {
                    case SpecialModelFamily::fence: {
                        const auto connects = [&](const Block neighbor) {
                            return blockId(neighbor) == modelId
                                || BlockUtil::isFenceGate(neighbor)
                                || BlockUtil::hasFullCubeCollision(neighbor);
                        };
                        const bool north = connects(neighborBlock(x, y, z - 1));
                        const bool south = connects(neighborBlock(x, y, z + 1));
                        const bool west = connects(neighborBlock(x - 1, y, z));
                        const bool east = connects(neighborBlock(x + 1, y, z));
                        box({6, 0, 6}, {10, 16, 10}, allFaces);
                        if (north) { box({7, 6, 0}, {9, 9, 8}, allFaces); box({7, 12, 0}, {9, 15, 8}, allFaces); }
                        if (south) { box({7, 6, 8}, {9, 9, 16}, allFaces); box({7, 12, 8}, {9, 15, 16}, allFaces); }
                        if (west) { box({0, 6, 7}, {8, 9, 9}, allFaces); box({0, 12, 7}, {8, 15, 9}, allFaces); }
                        if (east) { box({8, 6, 7}, {16, 9, 9}, allFaces); box({8, 12, 7}, {16, 15, 9}, allFaces); }
                        continue;
                    }
                    default: break;
                    }

                    switch (modelId) {
                    case blockId(BLOCK_COBBLESTONE_WALL): {
                        const auto connects = [&](const Block neighbor) {
                            return blockId(neighbor) == blockId(BLOCK_COBBLESTONE_WALL)
                                || BlockUtil::isFenceGate(neighbor)
                                || BlockUtil::hasFullCubeCollision(neighbor);
                        };
                        const bool north = connects(neighborBlock(x, y, z - 1));
                        const bool south = connects(neighborBlock(x, y, z + 1));
                        const bool west = connects(neighborBlock(x - 1, y, z));
                        const bool east = connects(neighborBlock(x + 1, y, z));
                        const bool straightNS = north && south && !west && !east;
                        const bool straightEW = west && east && !north && !south;
                        if (!straightNS && !straightEW) box({4, 0, 4}, {12, 16, 12}, allFaces);
                        if (north) box({5, 0, 0}, {11, 14, 8}, allFaces);
                        if (south) box({5, 0, 8}, {11, 14, 16}, allFaces);
                        if (west) box({0, 0, 5}, {8, 14, 11}, allFaces);
                        if (east) box({8, 0, 5}, {16, 14, 11}, allFaces);
                        if (!north && !south && !west && !east) box({4, 0, 4}, {12, 16, 12}, allFaces);
                        continue;
                    }
                    default: break;
                    }

                    switch (modelId) {
                    case blockId(BLOCK_ANVIL): {
                        const bool rotated = (blockMeta(block) & 1) != 0;
                        if (!rotated) {
                            box({2, 0, 2}, {14, 4, 14}, allFaces);
                            box({4, 4, 3}, {12, 5, 13}, allFaces);
                            box({6, 5, 4}, {10, 10, 12}, allFaces);
                            box({3, 10, 0}, {13, 16, 16}, allFaces);
                        } else {
                            box({2, 0, 2}, {14, 4, 14}, allFaces);
                            box({3, 4, 4}, {13, 5, 12}, allFaces);
                            box({4, 5, 6}, {12, 10, 10}, allFaces);
                            box({0, 10, 3}, {16, 16, 13}, allFaces);
                        }
                        continue;
                    }
                    default: break;
                    }
                }

                if (blockId(block) == blockId(BLOCK_TALLGRASS)) {
                    pushCross(meshData.vertices, x, y, z, getTileIndex(blockAtlas.second, block, 0),
                        tintData(block, 0, biomes[z * CHUNK_SIZE + x]), plantLighting(*this, x, y, z),
                        plantRenderOffset(chunkX * CHUNK_SIZE + x, chunkZ * CHUNK_SIZE + z, true));
                    continue;
                }

                if (blockId(block) == blockId(BLOCK_DOUBLE_PLANT)) {
                    const bool upper = (blockMeta(block) & 8) != 0;
                    uint8_t variant = blockMeta(block) & 7;
                    if (upper && y > 0) {
                        const Block lower = getBlock(x, y - 1, z);
                        if (blockId(lower) == blockId(BLOCK_DOUBLE_PLANT)) variant = blockMeta(lower) & 7;
                    }
                    const uint16_t tile = getNamedTileIndex(blockAtlas.second, doublePlantTexture(variant, upper));
                    const uint32_t tint = variant == 2 || variant == 3
                        ? tintData(BLOCK_TALLGRASS | 1, 0, biomes[z * CHUNK_SIZE + x])
                        : 1U << 31U;
                    pushCross(meshData.vertices, x, y, z, tile, tint, plantLighting(*this, x, y, z),
                        plantRenderOffset(chunkX * CHUNK_SIZE + x, chunkZ * CHUNK_SIZE + z, false));
                    continue;
                }

                // plants, crops, rails, torches, ladders and lily pads
                {
                    const uint16_t id = blockId(block);
                    const uint8_t meta = blockMeta(block);
                    const uint32_t light = plantLighting(*this, x, y, z);
                    constexpr uint32_t cutout = 1U << 31U; // alpha-tested layer, no biome color
                    const uint32_t foliageTint = tintData(BLOCK_TALLGRASS | 1, 0, biomes[z * CHUNK_SIZE + x]);
                    const std::string* texture = nullptr;
                    std::string name;
                    bool cross = false;
                    bool offsetXZ = false;
                    uint32_t crossTint = cutout;
                    switch (id) {
                        case blockId(BLOCK_SAPLING): {
                            static constexpr const char* names[]{"sapling_oak", "sapling_spruce", "sapling_birch",
                                "sapling_jungle", "sapling_acacia", "sapling_roofed_oak"};
                            name = names[std::min<int>(meta & 7, 5)]; cross = true; break;
                        }
                        case blockId(BLOCK_DEADBUSH): name = "deadbush"; cross = true; break;
                        case blockId(BLOCK_WEB): name = "web"; cross = true; break;
                        case blockId(BLOCK_YELLOW_FLOWER): name = "flower_dandelion"; cross = offsetXZ = true; break;
                        case blockId(BLOCK_RED_FLOWER): {
                            static constexpr const char* names[]{"flower_rose", "flower_blue_orchid", "flower_allium",
                                "flower_houstonia", "flower_tulip_red", "flower_tulip_orange", "flower_tulip_white",
                                "flower_tulip_pink", "flower_oxeye_daisy"};
                            name = names[std::min<int>(meta, 8)]; cross = offsetXZ = true; break;
                        }
                        case blockId(BLOCK_BROWN_MUSHROOM): name = "mushroom_brown"; cross = true; break;
                        case blockId(BLOCK_RED_MUSHROOM): name = "mushroom_red"; cross = true; break;
                        case blockId(BLOCK_REEDS): name = "reeds"; cross = true; crossTint = foliageTint; break;
                        default: break;
                    }
                    uint16_t tile = 0;
                    if (cross) {
                        if (tryNamedTile(blockAtlas.second, name, tile))
                            pushCross(meshData.vertices, x, y, z, tile, crossTint, light,
                                offsetXZ ? plantRenderOffset(chunkX * CHUNK_SIZE + x, chunkZ * CHUNK_SIZE + z, false) : plantNoOffset);
                        continue;
                    }

                    // crops: the "#" of four planes
                    std::string cropName;
                    switch (id) {
                        case blockId(BLOCK_WHEAT): cropName = "wheat_stage_" + std::to_string(meta & 7); break;
                        case blockId(BLOCK_CARROTS): {
                            static constexpr int stage[8]{0, 0, 1, 1, 2, 2, 2, 3};
                            cropName = "carrots_stage_" + std::to_string(stage[meta & 7]);
                            break;
                        }
                        case blockId(BLOCK_POTATOES): {
                            static constexpr int stage[8]{0, 0, 1, 1, 2, 2, 2, 3};
                            cropName = "potatoes_stage_" + std::to_string(stage[meta & 7]);
                            break;
                        }
                        case blockId(BLOCK_NETHER_WART): {
                            static constexpr int stage[4]{0, 1, 1, 2};
                            cropName = "nether_wart_stage_" + std::to_string(stage[meta & 3]);
                            break;
                        }
                        default: break;
                    }
                    if (!cropName.empty()) {
                        if (tryNamedTile(blockAtlas.second, cropName, tile)) {
                            for (const uint8_t position : {uint8_t(4), uint8_t(12)}) {
                                pushModelPlane(meshData.vertices, x, y, z, tile, cutout, light, 0,
                                    {{{position, 0, 0}, {position, 0, 16}, {position, 16, 16}, {position, 16, 0}}},
                                    {{{0, 16}, {16, 16}, {16, 0}, {0, 0}}});
                                pushModelPlane(meshData.vertices, x, y, z, tile, cutout, light, 2,
                                    {{{0, 0, position}, {16, 0, position}, {16, 16, position}, {0, 16, position}}},
                                    {{{0, 16}, {16, 16}, {16, 0}, {0, 0}}});
                            }
                        }
                        continue;
                    }

                    // rails: flat, sloped toward one side, or curved
                    bool rail = false;
                    bool powerRail = false;
                    std::string railName;
                    switch (id) {
                        case blockId(BLOCK_RAIL):
                            rail = true;
                            railName = (meta >= 6) ? "rail_normal_turned" : "rail_normal";
                            break;
                        case blockId(BLOCK_GOLDEN_RAIL):
                            rail = powerRail = true;
                            railName = (meta & 8) != 0 ? "rail_golden_powered" : "rail_golden";
                            break;
                        case blockId(BLOCK_DETECTOR_RAIL):
                            rail = powerRail = true;
                            railName = (meta & 8) != 0 ? "rail_detector_powered" : "rail_detector";
                            break;
                        case blockId(BLOCK_ACTIVATOR_RAIL):
                            rail = powerRail = true;
                            railName = (meta & 8) != 0 ? "rail_activator_powered" : "rail_activator";
                            break;
                        default: break;
                    }
                    if (rail) {
                        const int shape = powerRail ? (meta & 7) : meta;
                        if (tryNamedTile(blockAtlas.second, railName, tile)) {
                            // base model: tracks run north-south; ascending models rise toward the north `turns` quarter turns clockwise (seen from above) rotate it into place
                            int turns = 0;
                            bool raised = false;
                            switch (shape) {
                                case 0: turns = 0; break;       // north-south
                                case 1: turns = 1; break;       // east-west
                                case 2: turns = 1; raised = true; break;  // ascending east
                                case 3: turns = 3; raised = true; break;  // ascending west
                                case 4: turns = 0; raised = true; break;  // ascending north
                                case 5: turns = 2; raised = true; break;  // ascending south
                                case 6: turns = 0; break;       // south-east (the base curved model)
                                case 7: turns = 1; break;       // south-west
                                case 8: turns = 2; break;       // north-west
                                default: turns = 3; break;      // 9: north-east
                            }
                            std::array<std::array<uint8_t, 3>, 4> corners{};
                            std::array<std::array<uint8_t, 2>, 4> uvs{};
                            constexpr uint8_t baseX[4]{0, 16, 16, 0}, baseZ[4]{0, 0, 16, 16};
                            for (int corner = 0; corner < 4; ++corner) {
                                uint8_t cornerX = baseX[corner], cornerZ = baseZ[corner];
                                const uint8_t height = static_cast<uint8_t>(1 + (raised ? 16 - cornerZ : 0));
                                uvs[corner] = {cornerX, cornerZ};
                                for (int turn = 0; turn < turns; ++turn) {
                                    const uint8_t rotatedX = static_cast<uint8_t>(16 - cornerZ);
                                    cornerZ = cornerX;
                                    cornerX = rotatedX;
                                }
                                corners[corner] = {cornerX, height, cornerZ};
                            }
                            pushModelPlane(meshData.vertices, x, y, z, tile, cutout, light, 4, corners, uvs);
                        }
                        continue;
                    }

                    // ladders lie against the wall they are attached to
                    if (id == blockId(BLOCK_LADDER)) {
                        if (tryNamedTile(blockAtlas.second, "ladder", tile)) {
                            constexpr std::array<std::array<uint8_t, 2>, 4> ladderUv{{{0, 16}, {16, 16}, {16, 0}, {0, 0}}};
                            switch (meta) {
                                case 2: pushModelPlane(meshData.vertices, x, y, z, tile, cutout, light, 0,
                                    {{{16, 0, 15}, {0, 0, 15}, {0, 16, 15}, {16, 16, 15}}}, ladderUv); break;
                                case 3: pushModelPlane(meshData.vertices, x, y, z, tile, cutout, light, 1,
                                    {{{0, 0, 1}, {16, 0, 1}, {16, 16, 1}, {0, 16, 1}}}, ladderUv); break;
                                case 4: pushModelPlane(meshData.vertices, x, y, z, tile, cutout, light, 2,
                                    {{{15, 0, 0}, {15, 0, 16}, {15, 16, 16}, {15, 16, 0}}}, ladderUv); break;
                                default: pushModelPlane(meshData.vertices, x, y, z, tile, cutout, light, 3,
                                    {{{1, 0, 16}, {1, 0, 0}, {1, 16, 0}, {1, 16, 16}}}, ladderUv); break;
                            }
                        }
                        continue;
                    }

                    if (id == blockId(BLOCK_VINE)) {
                        if (tryNamedTile(blockAtlas.second, "vine", tile)) {
                            constexpr std::array<std::array<uint8_t, 2>, 4> vineUv{{{0, 16}, {16, 16}, {16, 0}, {0, 0}}};
                            const uint32_t vineTint = tintData(BLOCK_VINE, 0, biomes[z * CHUNK_SIZE + x]);
                            if ((meta & 1) != 0) pushModelPlane(meshData.vertices, x, y, z, tile, vineTint, light, 0,
                                {{{16, 0, 15}, {0, 0, 15}, {0, 16, 15}, {16, 16, 15}}}, vineUv);
                            if ((meta & 2) != 0) pushModelPlane(meshData.vertices, x, y, z, tile, vineTint, light, 3,
                                {{{1, 0, 16}, {1, 0, 0}, {1, 16, 0}, {1, 16, 16}}}, vineUv);
                            if ((meta & 4) != 0) pushModelPlane(meshData.vertices, x, y, z, tile, vineTint, light, 1,
                                {{{0, 0, 1}, {16, 0, 1}, {16, 16, 1}, {0, 16, 1}}}, vineUv);
                            if ((meta & 8) != 0) pushModelPlane(meshData.vertices, x, y, z, tile, vineTint, light, 2,
                                {{{15, 0, 0}, {15, 0, 16}, {15, 16, 16}, {15, 16, 0}}}, vineUv);
                            if (BlockUtil::hasFullCubeCollision(neighborBlock(x, y + 1, z)))
                                pushModelPlane(meshData.vertices, x, y, z, tile, vineTint, light, 4,
                                    {{{0, 15, 0}, {16, 15, 0}, {16, 15, 16}, {0, 15, 16}}}, vineUv);
                        }
                        continue;
                    }

                    // farmland: a 15/16 block with the dry or wet top
                    if (id == blockId(BLOCK_FARMLAND)) {
                        uint16_t dirt = 0, top = 0;
                        if (tryNamedTile(blockAtlas.second, "dirt", dirt)
                            && tryNamedTile(blockAtlas.second, meta > 0 ? "farmland_wet" : "farmland_dry", top)) {
                            std::array<uint16_t, 6> tiles{dirt, dirt, dirt, dirt, top, dirt};
                            std::array<std::array<uint32_t, 4>, 6> lighting{};
                            std::array<bool, 6> faces{};
                            for (int face = 0; face < 6; ++face) {
                                lighting[face] = smoothFaceLighting(*this, x, y, z, face);
                                int nx = x, ny = y, nz = z;
                                switch (face) {
                                    case 0: ++nz; break;
                                    case 1: --nz; break;
                                    case 2: --nx; break;
                                    case 3: ++nx; break;
                                    case 4: ++ny; break;
                                    case 5: --ny; break;
                                    default: break;
                                }
                                faces[face] = face == 4 || isNeighborOpaque(nx, ny, nz) != 1;
                            }
                            pushModelCuboid(meshData.vertices, x, y, z, tiles, 0, faces,
                                {0, 0, 0}, {16, 15, 16}, lighting);
                        }
                        continue;
                    }

                    // pumpkin and melon stems (stem_growthN / stem_fruit models): two diagonal planes showing the top rows of the stem texture, plus a bent upper stem toward a fruit
                    bool stem = false;
                    bool pumpkin = false;
                    switch (id) {
                        case blockId(BLOCK_PUMPKIN_STEM): stem = pumpkin = true; break;
                        case blockId(BLOCK_MELON_STEM): stem = true; break;
                        default: break;
                    }
                    if (stem) {
                        const Block fruit = pumpkin ? BLOCK_PUMPKIN : BLOCK_MELON_BLOCK;
                        static constexpr int sides[4][2]{{-1, 0}, {0, -1}, {1, 0}, {0, 1}}; // west, north, east, south
                        int connected = -1; // quarter turns clockwise from the base model, which bends west
                        for (int side = 0; side < 4; ++side)
                            if (blockId(neighborBlock(x + sides[side][0], y, z + sides[side][1])) == blockId(fruit))
                                connected = side;
                        const std::string base = pumpkin ? "pumpkin" : "melon";
                        uint16_t stemTile = 0, upperTile = 0;
                        if (tryNamedTile(blockAtlas.second, base + "_stem_disconnected", stemTile)) {
                            const uint32_t stemTint = foliageTint; // vanilla scales the color with the age
                            const int height = connected >= 0 ? 8 : std::min(2 + 2 * (meta & 7), 16);
                            const uint8_t top = static_cast<uint8_t>(height);
                            const std::array<std::array<uint8_t, 2>, 4> uvs{{{0, top}, {16, top}, {16, 0}, {0, 0}}};
                            pushModelPlane(meshData.vertices, x, y, z, stemTile, stemTint, light, 0,
                                {{{0, 0, 0}, {16, 0, 16}, {16, top, 16}, {0, top, 0}}}, uvs);
                            pushModelPlane(meshData.vertices, x, y, z, stemTile, stemTint, light, 2,
                                {{{0, 0, 16}, {16, 0, 0}, {16, top, 0}, {0, top, 16}}}, uvs);
                            if (connected >= 0 && tryNamedTile(blockAtlas.second, base + "_stem_connected", upperTile)) {
                                std::array<std::array<uint8_t, 3>, 4> corners{};
                                std::array<std::array<uint8_t, 2>, 4> upperUv{};
                                for (int corner = 0; corner < 4; ++corner) {
                                    constexpr uint8_t cornerY[4]{0, 0, 16, 16};
                                    constexpr uint8_t cornerX[4]{0, 9, 9, 0};
                                    uint8_t px = cornerX[corner], pz = 8;
                                    upperUv[corner] = {cornerX[corner], static_cast<uint8_t>(16 - cornerY[corner])};
                                    for (int turn = 0; turn < connected; ++turn) {
                                        const uint8_t rotatedX = static_cast<uint8_t>(16 - pz);
                                        pz = px;
                                        px = rotatedX;
                                    }
                                    corners[corner] = {px, cornerY[corner], pz};
                                }
                                pushModelPlane(meshData.vertices, x, y, z, upperTile, stemTint, light, 0, corners, upperUv);
                            }
                        }
                        continue;
                    }

                    // redstone dust: the cross texture cropped to its arms, or the line texture when it runs straight
                    if (id == blockId(BLOCK_REDSTONE_WIRE)) {
                        uint16_t crossTile = 0, lineTile = 0;
                        if (tryNamedTile(blockAtlas.second, "redstone_dust_cross", crossTile)
                            && tryNamedTile(blockAtlas.second, "redstone_dust_line", lineTile)) {
                            const auto powerSource = [](const Block other) {
                                switch (blockId(other)) {
                                    case blockId(BLOCK_LEVER): case blockId(BLOCK_STONE_BUTTON): case blockId(BLOCK_WOODEN_BUTTON):
                                    case blockId(BLOCK_STONE_PRESSURE_PLATE): case blockId(BLOCK_WOODEN_PRESSURE_PLATE):
                                    case blockId(BLOCK_LIGHT_WEIGHTED_PRESSURE_PLATE): case blockId(BLOCK_HEAVY_WEIGHTED_PRESSURE_PLATE):
                                    case blockId(BLOCK_REDSTONE_TORCH): case blockId(BLOCK_UNLIT_REDSTONE_TORCH):
                                    case blockId(BLOCK_DAYLIGHT_DETECTOR): case blockId(BLOCK_DAYLIGHT_DETECTOR_INVERTED):
                                    case blockId(BLOCK_TRIPWIRE_HOOK): case blockId(BLOCK_TRAPPED_CHEST):
                                    case blockId(BLOCK_DETECTOR_RAIL): case blockId(BLOCK_REDSTONE_BLOCK): return true;
                                    default: return false;
                                }
                            };
                            static constexpr int arms[4][2]{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}; // north east south west
                            bool linked[4]{};
                            for (int arm = 0; arm < 4; ++arm) {
                                const int dx = arms[arm][0], dz = arms[arm][1];
                                const Block side = neighborBlock(x + dx, y, z + dz);
                                bool link = blockId(side) == blockId(BLOCK_REDSTONE_WIRE) || powerSource(side);
                                if (!link && (blockId(side) == blockId(BLOCK_UNPOWERED_REPEATER)
                                    || blockId(side) == blockId(BLOCK_POWERED_REPEATER))) {
                                    // repeaters only connect along their own axis
                                    link = (dx != 0) == ((blockMeta(side) & 1) != 0);
                                }
                                // steps down to a wire below, and up a wall to a wire above
                                if (!link && !BlockUtil::isFullBlock(side)
                                    && blockId(neighborBlock(x + dx, y - 1, z + dz)) == blockId(BLOCK_REDSTONE_WIRE)) link = true;
                                if (!link && BlockUtil::isFullBlock(side)
                                    && blockId(neighborBlock(x + dx, y + 1, z + dz)) == blockId(BLOCK_REDSTONE_WIRE)
                                    && !BlockUtil::isFullBlock(neighborBlock(x, y + 1, z))) link = true;
                                linked[arm] = link;
                            }
                            // BlockRedstoneWire.colorMultiplier
                            const float power = static_cast<float>(meta & 15) / 15.0f;
                            const float red = meta == 0 ? 0.3f : std::clamp(power * 0.6f + 0.4f, 0.0f, 1.0f);
                            const float green = std::clamp(power * power * 0.7f - 0.5f, 0.0f, 1.0f);
                            const float blue = std::clamp(power * power * 0.6f - 0.7f, 0.0f, 1.0f);
                            const uint32_t dustTint = cutout | 6U | static_cast<uint32_t>(red * 255.0f) << 3U
                                | static_cast<uint32_t>(green * 255.0f) << 11U | static_cast<uint32_t>(blue * 255.0f) << 19U;
                            const auto rect = [&](const uint16_t texture, const uint8_t x0, const uint8_t z0,
                                                  const uint8_t x1, const uint8_t z1, const bool turned) {
                                const std::array<std::array<uint8_t, 3>, 4> corners{{{x0, 1, z0}, {x1, 1, z0}, {x1, 1, z1}, {x0, 1, z1}}};
                                std::array<std::array<uint8_t, 2>, 4> uvs{{{x0, z0}, {x1, z0}, {x1, z1}, {x0, z1}}};
                                if (turned) for (auto& uv : uvs) std::swap(uv[0], uv[1]);
                                pushModelPlane(meshData.vertices, x, y, z, texture, dustTint, light, 4, corners, uvs);
                            };
                            const bool northSouth = linked[0] || linked[2], eastWest = linked[1] || linked[3];
                            if (!northSouth && !eastWest) {
                                rect(crossTile, 5, 5, 11, 11, false);
                            } else if (northSouth != eastWest) {
                                rect(lineTile, 0, 0, 16, 16, northSouth); // a single run is drawn the full width
                            } else {
                                rect(crossTile, 5, 5, 11, 11, false);
                                if (linked[0]) rect(crossTile, 5, 0, 11, 5, false);
                                if (linked[1]) rect(crossTile, 11, 5, 16, 11, false);
                                if (linked[2]) rect(crossTile, 5, 11, 11, 16, false);
                                if (linked[3]) rect(crossTile, 0, 5, 5, 11, false);
                            }
                        }
                        continue;
                    }

                    // repeaters and comparators: a 2/16 slab with torches, turned to face the way they output
                    if (id == blockId(BLOCK_UNPOWERED_REPEATER) || id == blockId(BLOCK_POWERED_REPEATER)
                        || id == blockId(BLOCK_UNPOWERED_COMPARATOR) || id == blockId(BLOCK_POWERED_COMPARATOR)) {
                        const bool comparator = id == blockId(BLOCK_UNPOWERED_COMPARATOR) || id == blockId(BLOCK_POWERED_COMPARATOR);
                        const bool powered = id == blockId(BLOCK_POWERED_REPEATER) || id == blockId(BLOCK_POWERED_COMPARATOR);
                        uint16_t slabTile = 0, topTile = 0, torchOff = 0, torchOn = 0;
                        if (tryNamedTile(blockAtlas.second, "stone_slab_top", slabTile)
                            && tryNamedTile(blockAtlas.second, comparator ? (powered ? "comparator_on" : "comparator_off")
                                                                           : (powered ? "repeater_on" : "repeater_off"), topTile)
                            && tryNamedTile(blockAtlas.second, "redstone_torch_off", torchOff)
                            && tryNamedTile(blockAtlas.second, "redstone_torch_on", torchOn)) {
                            const int turns = meta & 3; // models face south; west, north and east are quarter turns
                            const auto turnPoint = [&](int px, int pz) {
                                for (int turn = 0; turn < turns; ++turn) { const int nx = 16 - pz; pz = px; px = nx; }
                                return std::array{px, pz};
                            };
                            std::array<std::array<uint32_t, 4>, 6> flat;
                            for (auto& face : flat) face.fill(light);
                            const auto addBox = [&](const std::array<int, 3>& low, const std::array<int, 3>& high,
                                                    const uint16_t texture, const std::array<bool, 6>& faces) {
                                const auto a = turnPoint(low[0], low[2]);
                                const auto b = turnPoint(high[0], high[2]);
                                const std::array<uint8_t, 3> from{static_cast<uint8_t>(std::min(a[0], b[0])), static_cast<uint8_t>(low[1]),
                                    static_cast<uint8_t>(std::min(a[1], b[1]))};
                                const std::array<uint8_t, 3> to{static_cast<uint8_t>(std::max(a[0], b[0])), static_cast<uint8_t>(high[1]),
                                    static_cast<uint8_t>(std::max(a[1], b[1]))};
                                pushModelCuboid(meshData.vertices, x, y, z, {texture, texture, texture, texture, texture, texture},
                                    cutout, faces, from, to, flat);
                            };
                            addBox({0, 0, 0}, {16, 2, 16}, slabTile, {true, true, true, true, false, true});
                            // the top is turned with the block, so its texture is mapped from the unturned corners
                            std::array<std::array<uint8_t, 3>, 4> corners{};
                            std::array<std::array<uint8_t, 2>, 4> uvs{};
                            for (int corner = 0; corner < 4; ++corner) {
                                constexpr int cornerZ[4]{0, 0, 16, 16}, cornerX[4]{0, 16, 16, 0};
                                const auto turned = turnPoint(cornerX[corner], cornerZ[corner]);
                                corners[corner] = {static_cast<uint8_t>(turned[0]), 2, static_cast<uint8_t>(turned[1])};
                                uvs[corner] = {static_cast<uint8_t>(cornerX[corner]), static_cast<uint8_t>(cornerZ[corner])};
                            }
                            pushModelPlane(meshData.vertices, x, y, z, topTile, cutout, light, 4, corners, uvs);
                            constexpr std::array allSides{true, true, true, true, true, true};
                            const uint16_t lit = powered ? torchOn : torchOff;
                            if (!comparator) {
                                const int delay = (meta >> 2 & 3) + 1;
                                addBox({7, 2, 2}, {9, 7, 4}, lit, allSides);
                                addBox({7, 2, 4 + 2 * delay}, {9, 7, 6 + 2 * delay}, lit, allSides);
                            } else {
                                addBox({4, 2, 11}, {6, 7, 13}, torchOff, allSides);
                                addBox({10, 2, 11}, {12, 7, 13}, torchOff, allSides);
                                if ((meta & 4) != 0) addBox({7, 2, 2}, {9, 6, 4}, torchOn, allSides); // subtract mode
                                else addBox({7, 2, 2}, {9, 4, 4}, lit, allSides);
                            }
                        }
                        continue;
                    }

                    // buttons: a 6x4 pad that sticks out 2/16 (1/16 when pressed)
                    if (id == blockId(BLOCK_STONE_BUTTON) || id == blockId(BLOCK_WOODEN_BUTTON)) {
                        if (tryNamedTile(blockAtlas.second, id == blockId(BLOCK_STONE_BUTTON) ? "stone" : "planks_oak", tile)) {
                            const uint8_t depth = (meta & 8) != 0 ? 1 : 2;
                            std::array<uint8_t, 3> low{5, 0, 6}, high{11, depth, 10};
                            switch (meta & 7) { // the side the button faces: 0 down, 1 east, 2 west, 3 south, 4 north, 5 up
                                case 0: low = {5, static_cast<uint8_t>(16 - depth), 6}; high = {11, 16, 10}; break;
                                case 1: low = {0, 6, 5}; high = {depth, 10, 11}; break;
                                case 2: low = {static_cast<uint8_t>(16 - depth), 6, 5}; high = {16, 10, 11}; break;
                                case 3: low = {5, 6, 0}; high = {11, 10, depth}; break;
                                case 4: low = {5, 6, static_cast<uint8_t>(16 - depth)}; high = {11, 10, 16}; break;
                                default: break;
                            }
                            std::array<std::array<uint32_t, 4>, 6> flat;
                            for (auto& face : flat) face.fill(light);
                            pushModelCuboid(meshData.vertices, x, y, z, {tile, tile, tile, tile, tile, tile}, cutout,
                                {true, true, true, true, true, true}, low, high, flat);
                        }
                        continue;
                    }

                    // levers: a cobblestone base and a stick leaning 45 degrees
                    if (id == blockId(BLOCK_LEVER)) {
                        uint16_t baseTile = 0, stickTile = 0;
                        if (tryNamedTile(blockAtlas.second, "cobblestone", baseTile)
                            && tryNamedTile(blockAtlas.second, "lever", stickTile)) {
                            const bool on = (meta & 8) != 0;
                            std::array<uint8_t, 3> low{5, 0, 4}, high{11, 3, 12};
                            // pivot of the stick, the way out of the mount, and the axis it swings along
                            glm::vec3 pivot{8, 1, 8}, outward{0, 1, 0}, swing{0, 0, 1};
                            switch (meta & 7) {
                                case 0: low = {5, 13, 4}; high = {11, 16, 12}; pivot = {8, 15, 8}; outward = {0, -1, 0}; swing = {1, 0, 0}; break;
                                case 7: low = {5, 13, 4}; high = {11, 16, 12}; pivot = {8, 15, 8}; outward = {0, -1, 0}; swing = {0, 0, 1}; break;
                                case 1: low = {0, 4, 5}; high = {3, 12, 11}; pivot = {1, 8, 8}; outward = {1, 0, 0}; swing = {0, 1, 0}; break;
                                case 2: low = {13, 4, 5}; high = {16, 12, 11}; pivot = {15, 8, 8}; outward = {-1, 0, 0}; swing = {0, 1, 0}; break;
                                case 3: low = {5, 4, 0}; high = {11, 12, 3}; pivot = {8, 8, 1}; outward = {0, 0, 1}; swing = {0, 1, 0}; break;
                                case 4: low = {5, 4, 13}; high = {11, 12, 16}; pivot = {8, 8, 15}; outward = {0, 0, -1}; swing = {0, 1, 0}; break;
                                case 6: low = {4, 0, 5}; high = {12, 3, 11}; swing = {1, 0, 0}; break;
                                default: break; // 5: on the floor, base running along z
                            }
                            std::array<std::array<uint32_t, 4>, 6> flat;
                            for (auto& face : flat) face.fill(light);
                            pushModelCuboid(meshData.vertices, x, y, z, {baseTile, baseTile, baseTile, baseTile, baseTile, baseTile},
                                cutout, {true, true, true, true, true, true}, low, high, flat);
                            const glm::vec3 direction = glm::normalize(outward + swing * (on ? 1.0f : -1.0f));
                            const glm::vec3 across = glm::normalize(glm::cross(direction, swing));
                            const glm::vec3 other = glm::cross(direction, across);
                            // a 2x2 stick, 10 long: four sides and the tip
                            const auto point = [&](const float along, const float a, const float b) {
                                const glm::vec3 position = pivot + direction * along + across * a + other * b;
                                return std::array<uint8_t, 3>{
                                    static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround(position.x)), 0, 31)),
                                    static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround(position.y)), 0, 31)),
                                    static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround(position.z)), 0, 31))};
                            };
                            constexpr float signs[4][2]{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
                            for (int side = 0; side < 4; ++side) {
                                const auto& from = signs[side];
                                const auto& to = signs[(side + 1) % 4];
                                pushModelPlane(meshData.vertices, x, y, z, stickTile, cutout, light, 4,
                                    {{point(0, from[0], from[1]), point(0, to[0], to[1]),
                                      point(10, to[0], to[1]), point(10, from[0], from[1])}},
                                    {{{7, 16}, {9, 16}, {9, 6}, {7, 6}}});
                            }
                            pushModelPlane(meshData.vertices, x, y, z, stickTile, cutout, light, 4,
                                {{point(10, -1, -1), point(10, 1, -1), point(10, 1, 1), point(10, -1, 1)}},
                                {{{7, 6}, {9, 6}, {9, 8}, {7, 8}}});
                        }
                        continue;
                    }

                    // lily pad
                    if (id == blockId(BLOCK_WATERLILY)) {
                        if (tryNamedTile(blockAtlas.second, "waterlily", tile))
                            pushModelPlane(meshData.vertices, x, y, z, tile,
                                tintData(BLOCK_LEAVES2, 0, biomes[z * CHUNK_SIZE + x]) | cutout, light, 4,
                                {{{0, 0, 0}, {16, 0, 0}, {16, 0, 16}, {0, 0, 16}}},
                                {{{0, 0}, {16, 0}, {16, 16}, {0, 16}}});
                        continue;
                    }

                    // torches: a 2x10 stick, leaning against its wall when mounted on one
                    const char* torchName = nullptr;
                    switch (id) {
                        case blockId(BLOCK_TORCH): torchName = "torch_on"; break;
                        case blockId(BLOCK_REDSTONE_TORCH): torchName = "redstone_torch_on"; break;
                        case blockId(BLOCK_UNLIT_REDSTONE_TORCH): torchName = "redstone_torch_off"; break;
                        default: break;
                    }
                    if (torchName != nullptr) {
                        if (tryNamedTile(blockAtlas.second, torchName, tile)) {
                            std::array<uint8_t, 3> low{7, 0, 7}, high{9, 10, 9};
                            switch (meta) { // 1 east, 2 west, 3 south, 4 north: the torch sits beside the wall
                                case 1: low = {1, 3, 7}; high = {3, 13, 9}; break;
                                case 2: low = {13, 3, 7}; high = {15, 13, 9}; break;
                                case 3: low = {7, 3, 1}; high = {9, 13, 3}; break;
                                case 4: low = {7, 3, 13}; high = {9, 13, 15}; break;
                                default: break;
                            }
                            std::array<std::array<uint32_t, 4>, 6> lighting;
                            for (auto& face : lighting) face.fill(light);
                            pushModelCuboid(meshData.vertices, x, y, z, {tile, tile, tile, tile, tile, tile}, cutout,
                                {true, true, true, true, true, false}, low, high, lighting);
                        }
                        continue;
                    }
                }

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

                    // an unavailable neighboring chunk must be treated as air. Deferring this face races the neighbor's construction and can permanently leave a hole if it is inserted just after pending faces are consumed
                    if (isNeighborOpaque(nx, ny, nz) == 1) continue;

                    const Block above = y + 1 < WORLD_HEIGHT ? getBlock(x, y + 1, z) : BLOCK_AIR;
                    const bool snowyGrass = blockId(block) == blockId(BLOCK_GRASS)
                        && (blockId(above) == blockId(BLOCK_SNOW)
                            || blockId(above) == blockId(BLOCK_SNOW_LAYER));
                    const bool snowedSide = snowyGrass && f < 4;
                    const uint16_t tile = snowedSide ? grassSideSnowed
                        : getTileIndex(blockAtlas.second, block, f);
                    const uint32_t tint = snowedSide ? 0
                        : tintData(block, f, biomes[z * CHUNK_SIZE + x], grassSideOverlay);
                    pushQuad(meshData.vertices, f, x, y, z, tile,
                        getRenderHeight(block), tint,
                        smoothFaceLighting(*this, x, y, z, f));
                }
            }
        }
    }

    // keep exact-size caches: this is the only CPU copy of the mesh
    sectionOpaque[section].assign(meshData.vertices.begin(), meshData.vertices.end());
    sectionTranslucent[section].assign(meshData.translucentVertices.begin(), meshData.translucentVertices.end());
    sectionOpaque[section].shrink_to_fit();
    sectionTranslucent[section].shrink_to_fit();
    sectionSkulls[section].swap(meshData.skullBlocks);
    }
    // the scratch buffers can be large; release them rather than keeping one per chunk
    std::vector<Vertex>().swap(meshData.vertices);
    std::vector<Vertex>().swap(meshData.translucentVertices);

    // section counts and skulls only; uploadMesh streams the section caches straight to the GPU
    meshData.skullBlocks.clear();
    for (int section = 0; section < WORLD_HEIGHT / CHUNK_SIZE; ++section) {
        meshData.sectionVertexCounts[section] = static_cast<GLsizei>(sectionOpaque[section].size());
        meshData.translucentSectionVertexCounts[section] = static_cast<GLsizei>(sectionTranslucent[section].size());
        meshData.skullBlocks.insert(meshData.skullBlocks.end(),
            sectionSkulls[section].begin(), sectionSkulls[section].end());
    }
}

void Chunk::uploadMesh() const {
    ZoneScopedN("Chunk GPU upload");
    std::lock_guard meshLock(meshMutex);

    if (VAO == 0) glGenVertexArrays(1, &VAO);
    if (VBO == 0) glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    size_t opaqueCount = 0, translucentCount = 0;
    for (int section = 0; section < WORLD_HEIGHT / CHUNK_SIZE; ++section) {
        opaqueCount += sectionOpaque[section].size();
        translucentCount += sectionTranslucent[section].size();
    }
    const GLsizeiptr opaqueSize = static_cast<GLsizeiptr>(opaqueCount * sizeof(Vertex));
    const GLsizeiptr translucentSize = static_cast<GLsizeiptr>(translucentCount * sizeof(Vertex));
    const GLsizeiptr uploadSize = opaqueSize + translucentSize;
    if (uploadSize > vboCapacity) {
        glBufferData(GL_ARRAY_BUFFER, uploadSize, nullptr, GL_STATIC_DRAW);
        vboCapacity = uploadSize;
    }
    GLsizeiptr offset = 0;
    for (const auto& vertices : sectionOpaque) {
        if (vertices.empty()) continue;
        const GLsizeiptr size = static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex));
        glBufferSubData(GL_ARRAY_BUFFER, offset, size, vertices.data());
        offset += size;
    }
    for (const auto& vertices : sectionTranslucent) {
        if (vertices.empty()) continue;
        const GLsizeiptr size = static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex));
        glBufferSubData(GL_ARRAY_BUFFER, offset, size, vertices.data());
        offset += size;
    }

    glVertexAttribIPointer(
        0,
        1,
        GL_UNSIGNED_INT,
        sizeof(Vertex),
        nullptr
    );
    glEnableVertexAttribArray(0);
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, tint)));
    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, shade)));
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, modelOffset)));
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(4, 1, GL_UNSIGNED_INT, sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, modelUv)));
    glEnableVertexAttribArray(4);

    glBindVertexArray(0);

    uploadedVertexCount = static_cast<GLsizei>(opaqueCount);
    uploadedTranslucentVertexCount = static_cast<GLsizei>(translucentCount);
    uploadedSectionVertexCounts = meshData.sectionVertexCounts;
    uploadedTranslucentSectionVertexCounts = meshData.translucentSectionVertexCounts;
    uploadedSkullBlocks = meshData.skullBlocks;
    loaded.store(true, std::memory_order_release);
    loadingMeshes.fetch_sub(1, std::memory_order_relaxed);
}

void Chunk::draw(const GLint chunkOffsetLocation, const glm::vec3& chunkOffset) const {
    if (uploadedVertexCount == 0) return;

    glUniform3fv(chunkOffsetLocation, 1, glm::value_ptr(chunkOffset));

    glBindVertexArray(VAO);
    std::array<GLint, WORLD_HEIGHT / CHUNK_SIZE> firsts{};
    std::array<GLsizei, WORLD_HEIGHT / CHUNK_SIZE> counts{};
    GLsizei drawCount = 0;
    GLint first = 0;
    for (size_t section = 0; section < uploadedSectionVertexCounts.size(); ++section) {
        const GLsizei count = uploadedSectionVertexCounts[section];
        if ((visibleSections & 1U << section) != 0 && count != 0) {
            firsts[drawCount] = first;
            counts[drawCount++] = count;
        }
        first += count;
    }
    if (drawCount != 0) glMultiDrawArrays(GL_TRIANGLES, firsts.data(), counts.data(), drawCount);
}

void Chunk::drawTranslucent(const GLint chunkOffsetLocation, const glm::vec3& chunkOffset) const {
    if (uploadedTranslucentVertexCount == 0) return;
    glUniform3fv(chunkOffsetLocation, 1, glm::value_ptr(chunkOffset));
    glBindVertexArray(VAO);
    std::array<GLint, WORLD_HEIGHT / CHUNK_SIZE> firsts{};
    std::array<GLsizei, WORLD_HEIGHT / CHUNK_SIZE> counts{};
    GLsizei drawCount = 0;
    GLint first = uploadedVertexCount;
    for (size_t section = 0; section < uploadedTranslucentSectionVertexCounts.size(); ++section) {
        const GLsizei count = uploadedTranslucentSectionVertexCounts[section];
        if ((visibleSections & 1U << section) != 0 && count != 0) {
            firsts[drawCount] = first;
            counts[drawCount++] = count;
        }
        first += count;
    }
    if (drawCount != 0) glMultiDrawArrays(GL_TRIANGLES, firsts.data(), counts.data(), drawCount);
}

void Chunk::queueMesh(const BlockAtlasData &blockAtlas, const bool priority,
                      const uint32_t sectionMask) const {
    dirtySections.fetch_or(sectionMask & ALL_SECTIONS, std::memory_order_acq_rel);
    meshDirty.store(true, std::memory_order_release);
    if (meshQueued.exchange(true, std::memory_order_acq_rel)) return;

    const std::shared_ptr<const Chunk> chunk = shared_from_this();
    Minecraft::chunkLightingThread.enqueue([chunk, &blockAtlas, priority] {
        while (true) {
            chunk->meshDirty.store(false, std::memory_order_release);
            const uint32_t sectionMask = chunk->dirtySections.exchange(0, std::memory_order_acq_rel);
            {
                std::lock_guard meshLock(chunk->meshMutex);
                chunk->generateMesh(blockAtlas, sectionMask);
            }
            if (!chunk->meshDirty.load(std::memory_order_acquire)) break;
            loadingMeshes.fetch_sub(1, std::memory_order_relaxed);
        }

        chunk->meshQueued.store(false, std::memory_order_release);
        if (chunk->meshDirty.load(std::memory_order_acquire)) chunk->queueMesh(blockAtlas, priority, 0);

        std::lock_guard lock(Minecraft::meshQueueMutex);
        MeshUploadJob upload{std::const_pointer_cast<Chunk>(chunk)};
        if (priority) Minecraft::meshUploadQueue.push_front(std::move(upload));
        else Minecraft::meshUploadQueue.push_back(std::move(upload));
    }, priority);
}
