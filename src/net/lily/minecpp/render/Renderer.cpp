#include "Renderer.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tracy/Tracy.hpp>
#include <random>

#include "BlockAtlas.hpp"
#include "Mipmap.hpp"
#include "net/lily/minecpp/Minecraft.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "lib/stb_image.hpp"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.hpp"
#include "net/lily/minecpp/util/ChatHistory.hpp"
#include "net/lily/minecpp/util/FrustumPlanes.hpp"
#include "net/lily/minecpp/client/entity/EntityOtherPlayerMP.hpp"
#include "net/lily/minecpp/entity/item/EntityItem.hpp"
#include "net/lily/minecpp/entity/EntityMob.hpp"
#include "net/lily/minecpp/util/ItemRegistry.hpp"

namespace {
float interpolateRotation(const float previous, const float current, const float partialTicks) {
    float difference = current - previous;
    while (difference < -180.0f) difference += 360.0f;
    while (difference >= 180.0f) difference -= 360.0f;
    return previous + partialTicks * difference;
}
double intersectCameraRay(const glm::vec3& start, const glm::vec3& delta, const AABB& bounds) {
    double minimum = 0.0;
    double maximum = 1.0;

    const auto clipAxis = [&](const double origin, const double direction,
                              const double lower, const double upper) {
        if (std::abs(direction) < 1.0E-8) return origin >= lower && origin <= upper;
        double first = (lower - origin) / direction;
        double second = (upper - origin) / direction;
        if (first > second) std::swap(first, second);
        minimum = std::max(minimum, first);
        maximum = std::min(maximum, second);
        return maximum >= minimum;
    };

    if (!clipAxis(start.x, delta.x, bounds.minX, bounds.maxX)) return 1.0;
    if (!clipAxis(start.y, delta.y, bounds.minY, bounds.maxY)) return 1.0;
    if (!clipAxis(start.z, delta.z, bounds.minZ, bounds.maxZ)) return 1.0;
    return std::clamp(minimum, 0.0, 1.0);
}

float clipThirdPersonDistance(const World* world, const glm::vec3& eye,
                              const glm::vec3& offset, const float distance) {
    const glm::vec3 end = eye + offset * distance;
    constexpr double cameraRadius = 0.1;
    const AABB searchBox(
        std::min(eye.x, end.x) - cameraRadius,
        std::min(eye.y, end.y) - cameraRadius,
        std::min(eye.z, end.z) - cameraRadius,
        std::max(eye.x, end.x) + cameraRadius,
        std::max(eye.y, end.y) + cameraRadius,
        std::max(eye.z, end.z) + cameraRadius);

    double fraction = 1.0;
    const glm::vec3 delta = offset * distance;
    for (const LocalBlock& collision : world->getCollidingBlocks(searchBox)) {
        const AABB expanded(
            collision.bounds.minX - cameraRadius,
            collision.bounds.minY - cameraRadius,
            collision.bounds.minZ - cameraRadius,
            collision.bounds.maxX + cameraRadius,
            collision.bounds.maxY + cameraRadius,
            collision.bounds.maxZ + cameraRadius);
        fraction = std::min(fraction, intersectCameraRay(eye, delta, expanded));
    }
    return static_cast<float>(fraction) * distance;
}

float biomeTemperature(uint8_t biome) {
    if (biome >= 128) biome -= 128;
    switch (biome) {
        case 2: case 17: case 37: case 38: case 39: return 2.0f;
        case 3: case 20: case 25: case 34: return 0.2f;
        case 4: case 18: case 27: case 29: return 0.7f;
        case 5: case 19: return 0.25f;
        case 6: return 0.8f;
        case 10: case 11: case 12: case 13: return 0.0f;
        case 14: case 15: return 0.9f;
        case 21: case 22: case 23: return 0.95f;
        case 26: return 0.05f;
        case 28: return 0.6f;
        case 30: case 31: return -0.5f;
        case 32: case 33: return 0.3f;
        case 35: case 36: return 1.2f;
        case 1: case 16: return 0.8f;
        default: return 0.5f;
    }
}

float biomeRainfall(uint8_t biome) {
    if (biome >= 128) biome -= 128;
    switch (biome) {
        case 2: case 17: case 35: case 36: case 37: case 38: case 39: return 0.0f;
        case 3: case 20: case 25: case 34: return 0.3f;
        case 4: case 18: case 27: case 28: case 29: return 0.8f;
        case 5: case 19: return 0.8f;
        case 6: case 21: case 22: case 23: return 0.9f;
        case 10: case 11: case 12: case 13: return 0.5f;
        case 14: case 15: return 1.0f;
        case 26: return 0.3f;
        case 30: case 31: return 0.4f;
        case 32: case 33: return 0.8f;
        case 1: case 16: return 0.4f;
        default: return 0.5f;
    }
}

glm::vec3 sampleBiomeColor(const uint8_t biome, const bool foliage) {
    static const TextureTile grass = BlockAtlas::loadPNG("assets/minecraft/textures/colormap/grass.png");
    static const TextureTile leaves = BlockAtlas::loadPNG("assets/minecraft/textures/colormap/foliage.png");
    const auto&[width, height, data] = foliage ? leaves : grass;
    const float temperature = std::clamp(biomeTemperature(biome), 0.0f, 1.0f);
    const float rainfall = std::clamp(biomeRainfall(biome), 0.0f, 1.0f) * temperature;
    const int x = std::clamp(static_cast<int>((1.0f - temperature) * 255.0f), 0, width - 1);
    const int y = std::clamp(static_cast<int>((1.0f - rainfall) * 255.0f), 0, height - 1);
    const size_t index = static_cast<size_t>((y * width + x) * 4);
    return glm::vec3(data[index], data[index + 1], data[index + 2]) / 255.0f;
}

glm::vec3 hsvToRgb(const float hue, const float saturation, const float value) {
    const float scaled = hue * 6.0f;
    const int sector = static_cast<int>(scaled) % 6;
    const float fraction = scaled - std::floor(scaled);
    const float p = value * (1.0f - saturation);
    const float q = value * (1.0f - fraction * saturation);
    const float t = value * (1.0f - (1.0f - fraction) * saturation);
    switch (sector) {
        case 0: return {value, t, p};
        case 1: return {q, value, p};
        case 2: return {p, value, t};
        case 3: return {p, q, value};
        case 4: return {t, p, value};
        case 5: return {value, p, q};
        default: return {};
    }
}
}

Renderer::~Renderer() {
    for (const auto& [url, skin] : skinTextures) {
        if (skin.texture != 0) glDeleteTextures(1, &skin.texture);
    }
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}

void Renderer::init() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    window = glfwCreateWindow(width, height, "MinecraftClient", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "Failed to initialize GLAD\n";
        exit(EXIT_FAILURE);
    }

    glfwSetWindowUserPointer(window, this);

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, const int fbWidth, const int fbHeight) {
        glViewport(0, 0, fbWidth, fbHeight);
        if (const auto* renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(window))) {
            renderer->updateProjection(fbWidth, fbHeight);
        }
    });

    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    blockShader = new Shader("assets/shaders/basic.vert", "assets/shaders/basic.frag");
    damageShader = new Shader("assets/shaders/damage.vert", "assets/shaders/damage.frag");
    selectionShader = new Shader("assets/shaders/selection.vert", "assets/shaders/selection.frag");
    handShader = new Shader("assets/shaders/hand.vert", "assets/shaders/hand.frag");
    particleShader = new Shader("assets/shaders/particle.vert", "assets/shaders/particle.frag");
    itemShader = new Shader("assets/shaders/item.vert", "assets/shaders/item.frag");
    itemSpriteShader = new Shader("assets/shaders/item_sprite.vert", "assets/shaders/item_sprite.frag");
    itemGlintShader = new Shader("assets/shaders/item_glint.vert", "assets/shaders/item_glint.frag");
    entityShadowShader = new Shader("assets/shaders/entity_shadow.vert", "assets/shaders/entity_shadow.frag");
    itemShader->use();
    itemShader->setInt("blockAtlas", 0);
    itemShader->setBool("useInstances", false);
    itemSpriteShader->use();
    itemSpriteShader->setInt("itemTexture", 0);
    itemSpriteShader->setBool("useInstances", false);
    itemSpriteShader->setFloat("opacity", 1.0f);
    itemSpriteShader->setVec2("uvMinimum", 0.0f, 0.0f);
    itemSpriteShader->setVec2("uvMaximum", 1.0f, 1.0f);
    itemGlintShader->use();
    itemGlintShader->setInt("glintTexture", 0);
    entityShadowShader->use();
    entityShadowShader->setInt("shadowTexture", 0);

    constexpr float itemQuad[] = {
        -0.5f,-0.5f,0.0f, 0.0f,1.0f,  0.5f,-0.5f,0.0f, 1.0f,1.0f,
         0.5f, 0.5f,0.0f, 1.0f,0.0f,  0.5f, 0.5f,0.0f, 1.0f,0.0f,
        -0.5f, 0.5f,0.0f, 0.0f,0.0f, -0.5f,-0.5f,0.0f, 0.0f,1.0f
    };
    glGenVertexArrays(1, &itemQuadVAO);
    glGenBuffers(1, &itemQuadVBO);
    glBindVertexArray(itemQuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, itemQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(itemQuad), itemQuad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
        reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    glGenVertexArrays(1, &entityShadowVAO);
    glGenBuffers(1, &entityShadowVBO);
    glBindVertexArray(entityShadowVAO);
    glBindBuffer(GL_ARRAY_BUFFER, entityShadowVBO);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
        reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
        reinterpret_cast<void*>(5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    damageShader->use();
    damageShader->setInt("blockAtlas", 0);

    chunkOffsetLoc = glGetUniformLocation(blockShader->ID, "chunkOffset");

    glGenVertexArrays(1, &particleVAO);
    glGenBuffers(1, &particleVBO);
    glBindVertexArray(particleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 9 * sizeof(float),
        reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float),
        reinterpret_cast<void*>(5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 9 * sizeof(float),
        reinterpret_cast<void*>(8 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glBindVertexArray(0);
    particleSheetTexture = loadTexture("assets/minecraft/textures/particle/particles.png");
    footprintTexture = loadTexture("assets/minecraft/textures/particle/footprint.png");
    explosionTexture = loadTexture("assets/minecraft/textures/entity/explosion.png");

    constexpr float vertices[] = {
        // positions          // texCoords
        -0.5f,-0.5f,-0.5f,   0.0f, 0.0f,
         0.5f,-0.5f,-0.5f,   1.0f, 0.0f,
         0.5f, 0.5f,-0.5f,   1.0f, 1.0f,
        -0.5f, 0.5f,-0.5f,   0.0f, 1.0f,
        -0.5f,-0.5f, 0.5f,   0.0f, 0.0f,
         0.5f,-0.5f, 0.5f,   1.0f, 0.0f,
         0.5f, 0.5f, 0.5f,   1.0f, 1.0f,
        -0.5f, 0.5f, 0.5f,   0.0f, 1.0f
    };
    constexpr unsigned int indices[] = {
        0,1,2, 2,3,0,
        4,5,6, 6,7,4,
        0,1,5, 5,4,0,
        2,3,7, 7,6,2,
        0,3,7, 7,4,0,
        1,2,6, 6,5,1
    };

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    unsigned int EBO;
    glGenBuffers(1, &EBO);

    glBindVertexArray(cubeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void *>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glGenVertexArrays(1, &stairSelectionVAO);
    glGenBuffers(1, &stairSelectionVBO);
    glBindVertexArray(stairSelectionVAO);
    glBindBuffer(GL_ARRAY_BUFFER, stairSelectionVBO);
    glBufferData(GL_ARRAY_BUFFER, 36 * 3 * sizeof(float), nullptr, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    std::vector<float> handVertices;
    handVertices.reserve(2 * 6 * 6 * 5);
    const auto handQuad = [&](const std::array<glm::vec3, 4>& positions,
                              const float u1, const float v1, const float u2, const float v2) {
        static constexpr int corners[6] = {0, 1, 2, 2, 3, 0};
        const glm::vec2 uvs[4]{{u2, v1}, {u1, v1}, {u1, v2}, {u2, v2}};
        for (const int corner : corners) {
            const glm::vec3& position = positions[corner];
            handVertices.insert(handVertices.end(), {position.x, position.y, position.z,
                uvs[corner].x / 64.0f, uvs[corner].y / 64.0f});
        }
    };
    const auto addRightArm = [&](const float expansion, const int textureY) {
        const float x0 = (-3.0f - expansion) / 16.0f;
        const float x1 = (1.0f + expansion) / 16.0f;
        const float y0 = (-2.0f - expansion) / 16.0f;
        const float y1 = (10.0f + expansion) / 16.0f;
        const float z0 = (-2.0f - expansion) / 16.0f;
        const float z1 = (2.0f + expansion) / 16.0f;
        const glm::vec3 v7{x0,y0,z0}, v{x1,y0,z0}, v1{x1,y1,z0}, v2{x0,y1,z0};
        const glm::vec3 v3{x0,y0,z1}, v4{x1,y0,z1}, v5{x1,y1,z1}, v6{x0,y1,z1};
        handQuad({{v4, v, v1, v5}}, 48, textureY + 4, 52, textureY + 16);
        handQuad({{v7, v3, v6, v2}}, 40, textureY + 4, 44, textureY + 16);
        handQuad({{v4, v3, v7, v}}, 44, textureY, 48, textureY + 4);
        handQuad({{v1, v2, v6, v5}}, 48, textureY + 4, 52, textureY);
        handQuad({{v, v7, v2, v1}}, 44, textureY + 4, 48, textureY + 16);
        handQuad({{v3, v4, v5, v6}}, 52, textureY + 4, 56, textureY + 16);
    };
    addRightArm(0.0f, 16);
    addRightArm(0.25f, 32);
    glGenVertexArrays(1, &handVAO);
    glGenBuffers(1, &handVBO);
    glBindVertexArray(handVAO);
    glBindBuffer(GL_ARRAY_BUFFER, handVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(handVertices.size() * sizeof(float)),
        handVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
        reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    std::vector<float> playerVertices;
    playerVertices.reserve(12 * 36 * 5);
    const auto addPlayerBox = [](std::vector<float>& vertices,
                                  const glm::vec3 minimum, const glm::vec3 maximum,
                                  const int textureX, const int textureY,
                                  const int textureWidth, const int textureHeight,
                                  const int textureDepth, const bool flipHorizontal = false,
                                  const bool rotateTextureHalfTurn = false,
                                  const bool armUvFix = false,
                                  const float textureCanvasHeight = 64.0f) {
        const glm::vec3 corners[8]{
            {minimum.x, minimum.y, minimum.z}, {maximum.x, minimum.y, minimum.z},
            {maximum.x, maximum.y, minimum.z}, {minimum.x, maximum.y, minimum.z},
            {minimum.x, minimum.y, maximum.z}, {maximum.x, minimum.y, maximum.z},
            {maximum.x, maximum.y, maximum.z}, {minimum.x, maximum.y, maximum.z},
        };
        static constexpr int faces[6][4]{
            {1, 0, 3, 2}, {4, 5, 6, 7}, {0, 4, 7, 3},
            {5, 1, 2, 6}, {3, 7, 6, 2}, {0, 1, 5, 4},
        };
        const int u0 = textureX;
        const int u1 = u0 + textureDepth;
        const int u2 = u1 + textureWidth;
        const int u3 = u2 + textureDepth;
        const int u4 = u3 + textureWidth;
        const int v0 = textureY;
        const int v1 = v0 + textureDepth;
        const int v2 = v1 + textureHeight;
        const int rectangles[6][4]{
            {u1, v1, u2, v2}, {u3, v1, u4, v2}, {u0, v1, u1, v2},
            {u2, v1, u3, v2}, {u1, v0, u2, v1}, {u2, v0, u2 + textureWidth, v1},
        };
        static constexpr int triangles[6]{0, 1, 2, 2, 3, 0};
        for (int face = 0; face < 6; ++face) {
            int textureFace = face;
            if (rotateTextureHalfTurn && face < 4) {
                static constexpr int oppositeFaces[4]{1, 0, 3, 2};
                textureFace = oppositeFaces[face];
            }
            const float uMin = rectangles[textureFace][0] / 64.0f;
            const float vMin = rectangles[textureFace][1] / textureCanvasHeight;
            const float uMax = rectangles[textureFace][2] / 64.0f;
            const float vMax = rectangles[textureFace][3] / textureCanvasHeight;
            const bool flipFace = flipHorizontal
                && (armUvFix ? face == 2 || face == 3 : face == 0 || face == 1);
            const float leftU = flipFace ? uMax : uMin;
            const float rightU = flipFace ? uMin : uMax;
            const glm::vec2 baseUvs[4]{{rightU, vMax}, {leftU, vMax},
                {leftU, vMin}, {rightU, vMin}};
            glm::vec2 uvs[4]{baseUvs[0], baseUvs[1], baseUvs[2], baseUvs[3]};
            if (armUvFix && face == 4) {
                uvs[0] = baseUvs[3];
                uvs[1] = baseUvs[0];
                uvs[2] = baseUvs[1];
                uvs[3] = baseUvs[2];
            }
            for (const int corner : triangles) {
                const glm::vec3& position = corners[faces[face][corner]];
                vertices.insert(vertices.end(), {position.x, position.y, position.z,
                    uvs[corner].x, uvs[corner].y});
            }
        }
    };
    constexpr float headLayer = 0.5f / 16.0f;
    constexpr float skinLayer = 0.25f / 16.0f;
    addPlayerBox(playerVertices, {-.25f, 0, -.25f}, {.25f, .5f, .25f}, 0, 0, 8, 8, 8);
    addPlayerBox(playerVertices, {-.25f-headLayer, -headLayer, -.25f-headLayer},
        {.25f+headLayer, .5f+headLayer, .25f+headLayer}, 32, 0, 8, 8, 8);
    addPlayerBox(playerVertices, {-.25f, -.75f, -.125f}, {.25f, 0, .125f}, 16, 16, 8, 12, 4, true);
    addPlayerBox(playerVertices, {-.25f-skinLayer, -.75f-skinLayer, -.125f-skinLayer},
        {.25f+skinLayer, skinLayer, .125f+skinLayer}, 16, 32, 8, 12, 4, true);
    addPlayerBox(playerVertices, {-.125f, -.75f, -.125f}, {.125f, 0, .125f}, 40, 16, 4, 12, 4, true, false, true);
    addPlayerBox(playerVertices, {-.125f-skinLayer, -.75f-skinLayer, -.125f-skinLayer},
        {.125f+skinLayer, skinLayer, .125f+skinLayer}, 40, 32, 4, 12, 4, true, false, true);
    addPlayerBox(playerVertices, {-.125f, -.75f, -.125f}, {.125f, 0, .125f}, 32, 48, 4, 12, 4, true, false, true);
    addPlayerBox(playerVertices, {-.125f-skinLayer, -.75f-skinLayer, -.125f-skinLayer},
        {.125f+skinLayer, skinLayer, .125f+skinLayer}, 48, 48, 4, 12, 4, true, false, true);
    addPlayerBox(playerVertices, {-.125f, -.75f, -.125f}, {.125f, 0, .125f}, 0, 16, 4, 12, 4);
    addPlayerBox(playerVertices, {-.125f-skinLayer, -.75f-skinLayer, -.125f-skinLayer},
        {.125f+skinLayer, skinLayer, .125f+skinLayer}, 0, 32, 4, 12, 4);
    addPlayerBox(playerVertices, {-.125f, -.75f, -.125f}, {.125f, 0, .125f}, 16, 48, 4, 12, 4);
    addPlayerBox(playerVertices, {-.125f-skinLayer, -.75f-skinLayer, -.125f-skinLayer},
        {.125f+skinLayer, skinLayer, .125f+skinLayer}, 0, 48, 4, 12, 4);
    glGenVertexArrays(1, &playerVAO);
    glGenBuffers(1, &playerVBO);
    glBindVertexArray(playerVAO);
    glBindBuffer(GL_ARRAY_BUFFER, playerVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(playerVertices.size() * sizeof(float)),
        playerVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
        reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    std::vector<float> skullVertices;
    skullVertices.reserve(36 * 5);
    addPlayerBox(skullVertices, {-.25f, -.25f, -.25f}, {.25f, .25f, .25f}, 0, 0, 8, 8, 8,
        false, false, false, 32.0f);
    glGenVertexArrays(1, &skullVAO);
    glGenBuffers(1, &skullVBO);
    glBindVertexArray(skullVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skullVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(skullVertices.size() * sizeof(float)),
        skullVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
        reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    std::vector<float> slimPlayerVertices;
    slimPlayerVertices.reserve(playerVertices.size());
    const auto appendBox = [&](const glm::vec3 minimum, const glm::vec3 maximum,
                               const int textureX, const int textureY,
                               const int textureWidth, const int textureHeight,
                               const int textureDepth, const bool flipHorizontal = false,
                               const bool rotateTextureHalfTurn = false,
                               const bool armUvFix = false) {
        addPlayerBox(slimPlayerVertices, minimum, maximum, textureX, textureY,
            textureWidth, textureHeight, textureDepth, flipHorizontal,
            rotateTextureHalfTurn, armUvFix);
    };
    appendBox({-.25f, 0, -.25f}, {.25f, .5f, .25f}, 0, 0, 8, 8, 8);
    appendBox({-.25f-headLayer, -headLayer, -.25f-headLayer},
        {.25f+headLayer, .5f+headLayer, .25f+headLayer}, 32, 0, 8, 8, 8);
    appendBox({-.25f, -.75f, -.125f}, {.25f, 0, .125f}, 16, 16, 8, 12, 4, true);
    appendBox({-.25f-skinLayer, -.75f-skinLayer, -.125f-skinLayer},
        {.25f+skinLayer, skinLayer, .125f+skinLayer}, 16, 32, 8, 12, 4, true);
    appendBox({-.125f, -.75f, -.125f}, {.0625f, 0, .125f}, 40, 16, 3, 12, 4, true, true, true);
    appendBox({-.125f-skinLayer, -.75f-skinLayer, -.125f-skinLayer},
        {.0625f+skinLayer, skinLayer, .125f+skinLayer}, 40, 32, 3, 12, 4, true, true, true);
    appendBox({-.0625f, -.75f, -.125f}, {.125f, 0, .125f}, 32, 48, 3, 12, 4, true, true, true);
    appendBox({-.0625f-skinLayer, -.75f-skinLayer, -.125f-skinLayer},
        {.125f+skinLayer, skinLayer, .125f+skinLayer}, 48, 48, 3, 12, 4, true, true, true);
    appendBox({-.125f, -.75f, -.125f}, {.125f, 0, .125f}, 0, 16, 4, 12, 4);
    appendBox({-.125f-skinLayer, -.75f-skinLayer, -.125f-skinLayer},
        {.125f+skinLayer, skinLayer, .125f+skinLayer}, 0, 32, 4, 12, 4);
    appendBox({-.125f, -.75f, -.125f}, {.125f, 0, .125f}, 16, 48, 4, 12, 4);
    appendBox({-.125f-skinLayer, -.75f-skinLayer, -.125f-skinLayer},
        {.125f+skinLayer, skinLayer, .125f+skinLayer}, 0, 48, 4, 12, 4);
    glGenVertexArrays(1, &slimPlayerVAO);
    glGenBuffers(1, &slimPlayerVBO);
    glBindVertexArray(slimPlayerVAO);
    glBindBuffer(GL_ARRAY_BUFFER, slimPlayerVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(slimPlayerVertices.size() * sizeof(float)),
        slimPlayerVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
        reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // modelBiped(1.0) for helmet/chestplate/boots and ModelBiped(0.5) for leggings, 64x32 armor textures per model: head, headwear, body, right arm, left arm, right leg, left leg (36 vertices each)
    std::vector<float> armorVertices;
    for (const float inflatePixels : {1.0f, 0.5f}) {
        const float i = inflatePixels / 16.0f;
        const auto mirrored = [](std::vector<float> box) {
            for (size_t index = 0; index < box.size(); index += 5) box[index] = -box[index];
            return box;
        };
        const auto append = [&](const std::vector<float>& box) {
            armorVertices.insert(armorVertices.end(), box.begin(), box.end());
        };
        std::vector<float> head, headwear, body, arm, leg;
        addPlayerBox(head, {-.25f-i, -i, -.25f-i}, {.25f+i, .5f+i, .25f+i}, 0, 0, 8, 8, 8,
            false, false, false, 32.0f);
        const float hat = i + 0.5f / 16.0f;
        addPlayerBox(headwear, {-.25f-hat, -hat, -.25f-hat}, {.25f+hat, .5f+hat, .25f+hat}, 32, 0, 8, 8, 8,
            false, false, false, 32.0f);
        addPlayerBox(body, {-.25f-i, -.75f-i, -.125f-i}, {.25f+i, i, .125f+i}, 16, 16, 8, 12, 4,
            true, false, false, 32.0f);
        addPlayerBox(arm, {-.125f-i, -.75f-i, -.125f-i}, {.125f+i, i, .125f+i}, 40, 16, 4, 12, 4,
            true, false, true, 32.0f);
        addPlayerBox(leg, {-.125f-i, -.75f-i, -.125f-i}, {.125f+i, i, .125f+i}, 0, 16, 4, 12, 4,
            false, false, false, 32.0f);
        // the helmet's and boots' left/right faces (2 and 3) need their U flipped
        const auto flipFaces = [](std::vector<float>& box, const std::initializer_list<int> faces) {
            for (const int face : faces) {
                float low = 1.0f, high = 0.0f;
                for (int vertex = 0; vertex < 6; ++vertex) {
                    const float u = box[(face * 6 + vertex) * 5 + 3];
                    low = std::min(low, u);
                    high = std::max(high, u);
                }
                for (int vertex = 0; vertex < 6; ++vertex) {
                    float& u = box[(face * 6 + vertex) * 5 + 3];
                    u = low + high - u;
                }
            }
        };
        flipFaces(head, {0, 1, 2, 3});
        flipFaces(headwear, {0, 1, 2, 3});
        if (inflatePixels == 1.0f) flipFaces(leg, {0, 1, 2, 3}); // boots (front 0, back 1, sides 2/3); leggings use the 0.5 model
        append(head); append(headwear); append(body);
        append(arm); append(mirrored(arm)); // left limbs are the right limbs mirrored, as in vanilla
        append(leg); append(mirrored(leg));
    }
    glGenVertexArrays(1, &armorVAO);
    glGenBuffers(1, &armorVBO);
    glBindVertexArray(armorVAO);
    glBindBuffer(GL_ARRAY_BUFFER, armorVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(armorVertices.size() * sizeof(float)),
        armorVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
        reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    constexpr float selectionVertices[] = {
        0,0,0, 1,0,0,  1,0,0, 1,0,1,  1,0,1, 0,0,1,  0,0,1, 0,0,0,
        0,1,0, 1,1,0,  1,1,0, 1,1,1,  1,1,1, 0,1,1,  0,1,1, 0,1,0,
        0,0,0, 0,1,0,  1,0,0, 1,1,0,  1,0,1, 1,1,1,  0,0,1, 0,1,1,
    };
    glGenVertexArrays(1, &selectionVAO);
    glGenBuffers(1, &selectionVBO);
    glBindVertexArray(selectionVAO);
    glBindBuffer(GL_ARRAY_BUFFER, selectionVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(selectionVertices), selectionVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    static constexpr unsigned int damageFaces[6][4] = {
        {3, 2, 6, 7}, {1, 0, 4, 5}, {0, 3, 7, 4},
        {2, 1, 5, 6}, {4, 7, 6, 5}, {0, 1, 2, 3}
    };
    static constexpr float damagePositions[8][3] = {
        {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f},
        {0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, 0.5f},
        {-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f},
        {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}
    };
    static constexpr unsigned int damageCorners[6] = {0, 1, 2, 2, 3, 0};
    static constexpr float damageUvs[4][2] = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}
    };
    std::vector<float> damageVertices;
    damageVertices.reserve(6 * 6 * 6);
    for (unsigned int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        const auto& face = damageFaces[faceIndex];
        for (const unsigned int corner : damageCorners) {
            const auto& position = damagePositions[face[corner]];
            const auto& uv = damageUvs[corner];
            damageVertices.insert(damageVertices.end(),
                {position[0], position[1], position[2], uv[0], uv[1],
                 static_cast<float>(faceIndex)});
        }
    }
    static constexpr float crossPlanes[2][4][3] = {
        {{-0.45f, -0.5f, -0.45f}, {0.45f, -0.5f, 0.45f},
         {0.45f, 0.5f, 0.45f}, {-0.45f, 0.5f, -0.45f}},
        {{-0.45f, -0.5f, 0.45f}, {0.45f, -0.5f, -0.45f},
         {0.45f, 0.5f, -0.45f}, {-0.45f, 0.5f, 0.45f}},
    };
    for (const auto& plane : crossPlanes) {
        for (const unsigned int corner : damageCorners) {
            const auto& position = plane[corner];
            const auto& uv = damageUvs[corner];
            damageVertices.insert(damageVertices.end(),
                {position[0], position[1], position[2], uv[0], uv[1], 6.0f});
        }
    }
    // item models after the cube (0-35) and the cross planes (36-47): stairs (48-119, a slab plus the upper step on the +x side) and a half slab (120-155). UVs follow the cube face they belong to
    const auto addItemBox = [&](const float minX, const float minY, const float minZ,
                                const float maxX, const float maxY, const float maxZ) {
        for (unsigned int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            const auto& face = damageFaces[faceIndex];
            const auto& origin = damagePositions[face[0]];
            const auto& uEnd = damagePositions[face[1]];
            const auto& vEnd = damagePositions[face[3]];
            float uAxis[3], vAxis[3], uLength = 0.0f, vLength = 0.0f;
            for (int axis = 0; axis < 3; ++axis) {
                uAxis[axis] = uEnd[axis] - origin[axis];
                vAxis[axis] = vEnd[axis] - origin[axis];
                uLength += uAxis[axis] * uAxis[axis];
                vLength += vAxis[axis] * vAxis[axis];
            }
            for (const unsigned int corner : damageCorners) {
                const unsigned int index = face[corner];
                const float position[3]{
                    (index == 1 || index == 2 || index == 5 || index == 6) ? maxX : minX,
                    index >= 4 ? maxY : minY,
                    (index == 2 || index == 3 || index == 6 || index == 7) ? maxZ : minZ};
                float u = 0.0f, v = 0.0f;
                for (int axis = 0; axis < 3; ++axis) {
                    u += (position[axis] - origin[axis]) * uAxis[axis];
                    v += (position[axis] - origin[axis]) * vAxis[axis];
                }
                damageVertices.insert(damageVertices.end(), {position[0], position[1], position[2],
                    u / uLength, v / vLength, static_cast<float>(faceIndex)});
            }
        }
    };
    addItemBox(-0.5f, -0.5f, -0.5f, 0.5f, 0.0f, 0.5f);   // stair: lower half
    addItemBox(0.0f, 0.0f, -0.5f, 0.5f, 0.5f, 0.5f);     // stair: upper step
    addItemBox(-0.5f, -0.5f, -0.5f, 0.5f, 0.0f, 0.5f);   // half slab
    glGenVertexArrays(1, &damageVAO);
    glGenBuffers(1, &damageVBO);
    glBindVertexArray(damageVAO);
    glBindBuffer(GL_ARRAY_BUFFER, damageVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(damageVertices.size() * sizeof(float)),
        damageVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
        reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
        reinterpret_cast<void*>(5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    const auto addAnvilBox = [&](std::vector<float>& vertices,
                                  const glm::vec3& minimum, const glm::vec3& maximum,
                                  const int narrowUvFace = -1, const float narrowU0 = 0.0f, const float narrowU1 = 1.0f) {
        const float positions[8][3] = {
            {minimum.x, minimum.y, minimum.z}, {maximum.x, minimum.y, minimum.z},
            {maximum.x, minimum.y, maximum.z}, {minimum.x, minimum.y, maximum.z},
            {minimum.x, maximum.y, minimum.z}, {maximum.x, maximum.y, minimum.z},
            {maximum.x, maximum.y, maximum.z}, {minimum.x, maximum.y, maximum.z}
        };
        for (unsigned int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            const auto& face = damageFaces[faceIndex];
            for (const unsigned int corner : damageCorners) {
                const auto& position = positions[face[corner]];
                const auto& uv = damageUvs[corner];
                // the anvil's narrow top-face texture excludes transparent padding columns, so remap u into that sub-rect
                const float u = static_cast<int>(faceIndex) == narrowUvFace ? narrowU0 + uv[0] * (narrowU1 - narrowU0) : uv[0];
                vertices.insert(vertices.end(),
                    {position[0], position[1], position[2], u, uv[1],
                     static_cast<float>(faceIndex)});
            }
        }
    };
    std::vector<float> anvilVertices;
    anvilVertices.reserve(36 * 4 * 6);
    // exact vanilla 1.8.9 anvil block model (models/block/anvil.json), coords /16 and recentered to [-0.5,0.5]^3
    addAnvilBox(anvilVertices, {-0.375f, -0.5f, -0.375f}, {0.375f, -0.25f, 0.375f}); // base
    addAnvilBox(anvilVertices, {-0.25f, -0.25f, -0.3125f}, {0.25f, -0.1875f, 0.3125f}); // lower narrow portion
    addAnvilBox(anvilVertices, {-0.125f, -0.1875f, -0.25f}, {0.125f, 0.125f, 0.25f}); // wider section beneath top
    addAnvilBox(anvilVertices, {-0.3125f, 0.125f, -0.5f}, {0.3125f, 0.5f, 0.5f}, 4, 3.0f / 16.0f, 13.0f / 16.0f); // top
    glGenVertexArrays(1, &anvilVAO);
    glGenBuffers(1, &anvilVBO);
    glBindVertexArray(anvilVAO);
    glBindBuffer(GL_ARRAY_BUFFER, anvilVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(anvilVertices.size() * sizeof(float)),
        anvilVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
        reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
        reinterpret_cast<void*>(5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    enum MobAnimation : uint8_t {
        MOB_STATIC, MOB_HEAD, MOB_LEG_A, MOB_LEG_B, MOB_ARM_A, MOB_ARM_B,
        MOB_RABBIT_THIGH, MOB_RABBIT_FOOT, MOB_RABBIT_SHIN,
        MOB_STAND_HEAD, MOB_STAND_BODY, MOB_STAND_LEFT_ARM, MOB_STAND_RIGHT_ARM,
        MOB_STAND_LEFT_LEG, MOB_STAND_RIGHT_LEG, MOB_STAND_BODY_POST, MOB_STAND_BASE
    };
    const auto buildMobMesh = [&](const uint16_t type, const auto& addParts) {
        std::vector<float> vertices;
        std::vector<glm::vec3> vanillaPivots;
        MobMesh mesh;
        const auto addModelBox = [&](const glm::vec3 box, const glm::vec3 size,
                                     const int textureX, const int textureY,
                                     const float canvasHeight, const float inflate,
                                     const bool mirror) {
            float x0 = box.x - inflate;
            float x1 = box.x + size.x + inflate;
            const float y0 = box.y - inflate;
            const float y1 = box.y + size.y + inflate;
            const float z0 = box.z - inflate;
            const float z1 = box.z + size.z + inflate;
            if (mirror) std::swap(x0, x1);
            const glm::vec3 points[8]{
                {x0,-y0,z0}, {x1,-y0,z0}, {x1,-y1,z0}, {x0,-y1,z0},
                {x0,-y0,z1}, {x1,-y0,z1}, {x1,-y1,z1}, {x0,-y1,z1}
            };
            static constexpr int faces[6][4]{
                {5,1,2,6}, {0,4,7,3}, {5,4,0,1},
                {2,3,7,6}, {1,0,3,2}, {4,5,6,7}
            };
            const float depth = size.z;
            const float width = size.x;
            const float height = size.y;
            const float rectangles[6][4]{
                {depth+width,depth,depth+width+depth,depth+height},
                {0,depth,depth,depth+height},
                {depth,0,depth+width,depth},
                {depth+width,depth,depth+width+width,0},
                {depth,depth,depth+width,depth+height},
                {depth+width+depth,depth,depth+width+depth+width,depth+height}
            };
            static constexpr int triangles[6]{0,1,2,2,3,0};
            for (int face = 0; face < 6; ++face) {
                // canvasHeight doubles as the texture size: 65 is a 128x64 texture (double chests)
                const float canvasWidth = canvasHeight >= 128.0f || canvasHeight == 65.0f ? 128.0f : 64.0f;
                const float canvasTall = canvasHeight == 65.0f ? 64.0f : canvasHeight;
                const float u0 = (textureX + rectangles[face][0]) / canvasWidth;
                const float v0 = (textureY + rectangles[face][1]) / canvasTall;
                const float u1 = (textureX + rectangles[face][2]) / canvasWidth;
                const float v1 = (textureY + rectangles[face][3]) / canvasTall;
                const glm::vec2 uvs[4]{{u0,v0},{u1,v0},{u1,v1},{u0,v1}};
                for (const int corner : triangles) {
                    const int modelCorner = mirror ? 3 - corner : corner;
                    const glm::vec3 point = points[faces[face][modelCorner]] / 16.0f;
                    const glm::vec2 uv = uvs[modelCorner];
                    vertices.insert(vertices.end(), {point.x,point.y,point.z,uv.x,uv.y});
                }
            }
        };
        const auto addPart = [&](const glm::vec3 box, const glm::vec3 size,
                                 const glm::vec3 pivot, const int textureX, const int textureY,
                                 const glm::vec3 rotation, const uint8_t animation,
                                 const float canvasHeight = 32.0f, const float inflate = 0.0f,
                                 const bool mirror = false, const bool vanillaModelBox = false,
                                 const int parent = -1) {
            glm::vec3 pivotOffset = pivot;
            vanillaPivots.push_back(pivot);
            if (parent >= 0) pivotOffset = pivot - vanillaPivots[static_cast<size_t>(parent)];
            const glm::vec3 meshPivot = parent >= 0
                ? glm::vec3{pivotOffset.x / 16.0f, -pivotOffset.y / 16.0f, pivotOffset.z / 16.0f}
                : glm::vec3{pivot.x / 16.0f, (24.0f - pivot.y) / 16.0f, pivot.z / 16.0f};
            mesh.parts.push_back({static_cast<int>(vertices.size() / 5), meshPivot, rotation, animation, parent});
            // modelBox's exact UV layout. Our mob frame is vanilla's mirrored in X (no scale(-1,-1,1)), so parts written in vanilla coordinates need the mirror flag toggled; the armor stand's parts are already authored in mirrored coordinates
            addModelBox(box, size, textureX, textureY, canvasHeight, inflate,
                vanillaModelBox ? mirror : !mirror);
        };
        addParts(addPart);
        glGenVertexArrays(1, &mesh.vao);
        glGenBuffers(1, &mesh.vbo);
        glBindVertexArray(mesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
            vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
            reinterpret_cast<void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
        mobMeshes.emplace(type, std::move(mesh));
    };
    buildMobMesh(30, [&](const auto& part) {
        part({-1,-7,-1}, {2,7,2}, {0,1,0}, 0,0, {}, MOB_STAND_HEAD, 64,0,false,true);
        part({-6,0,-1.5f}, {12,3,3}, {0,0,0}, 0,26, {}, MOB_STAND_BODY, 64,0,false,true);
        part({0,-2,-1}, {2,12,2}, {5,2,0}, 24,0, {}, MOB_STAND_RIGHT_ARM, 64,0,true,true);
        part({-2,-2,-1}, {2,12,2}, {-5,2,0}, 32,16, {}, MOB_STAND_LEFT_ARM, 64,0,false,true);
        part({-1,0,-1}, {2,11,2}, {1.9f,12,0}, 8,0, {}, MOB_STAND_RIGHT_LEG, 64,0,false,true);
        part({-1,0,-1}, {2,11,2}, {-1.9f,12,0}, 40,16, {}, MOB_STAND_LEFT_LEG, 64,0,true,true);
        part({-3,3,-1}, {2,7,2}, {0,0,0}, 16,0, {}, MOB_STAND_BODY_POST, 64,0,false,true);
        part({1,3,-1}, {2,7,2}, {0,0,0}, 48,16, {}, MOB_STAND_BODY_POST, 64,0,false,true);
        part({-4,10,-1}, {8,2,2}, {0,0,0}, 0,48, {}, MOB_STAND_BODY_POST, 64,0,false,true);
        part({-6,11,-6}, {12,1,12}, {0,12,0}, 0,32, {}, MOB_STAND_BASE, 64,0,false,true);
    });
    const auto buildQuadruped = [&](const uint8_t type, const int legHeight,
                                    const bool cow, const bool sheep) {
        buildMobMesh(type, [&](const auto& part) {
            if (cow) {
                part({-4,-4,-6}, {8,8,6}, {0,4,-8}, 0,0, {}, MOB_HEAD);
                part({-6,-10,-7}, {12,18,10}, {0,5,2}, 18,4,
                    {-1.57079632679f,0,0}, MOB_STATIC);
            } else if (sheep) {
                // modelSheep2 (shorn sheep); the wool is a separate inflated ModelSheep1 layer
                part({-3,-4,-6}, {6,6,8}, {0,6,-8}, 0,0, {}, MOB_HEAD);
                part({-4,-10,-7}, {8,16,6}, {0,5,2}, 28,8,
                    {-1.57079632679f,0,0}, MOB_STATIC);
            } else {
                // modelQuadruped: head y = 18 - legHeight, body y = 17 - legHeight
                part({-4,-4,-8}, {8,8,8}, {0,18.0f-legHeight,-6}, 0,0, {}, MOB_HEAD);
                if (type == 90) // modelPig snout
                    part({-2,0,-9}, {4,3,1}, {0,18.0f-legHeight,-6}, 16,16, {}, MOB_HEAD);
                part({-5,-10,-7}, {10,16,8}, {0,17.0f-legHeight,2}, 28,8,
                    {-1.57079632679f,0,0}, MOB_STATIC);
            }
            const float backX = cow ? 4.0f : 3.0f;
            part({-2,0,-2}, {4,legHeight,4}, {-backX,24-legHeight,7}, 0,16, {}, MOB_LEG_A);
            part({-2,0,-2}, {4,legHeight,4}, { backX,24-legHeight,7}, 0,16, {}, MOB_LEG_B);
            part({-2,0,-2}, {4,legHeight,4}, {-backX,24-legHeight,-5}, 0,16, {}, MOB_LEG_B);
            part({-2,0,-2}, {4,legHeight,4}, { backX,24-legHeight,-5}, 0,16, {}, MOB_LEG_A);
        });
    };
    buildQuadruped(90, 6, false, false);
    buildQuadruped(91, 12, false, true);
    // modelSheep1: inflated wool layer drawn over the shorn sheep with sheep_fur.png
    buildMobMesh(250, [&](const auto& part) {
        part({-3,-4,-4}, {6,6,6}, {0,6,-8}, 0,0, {}, MOB_HEAD, 32.0f, 0.6f);
        part({-4,-10,-7}, {8,16,6}, {0,5,2}, 28,8, {-1.57079632679f,0,0}, MOB_STATIC, 32.0f, 1.75f);
        part({-2,0,-2}, {4,6,4}, {-3,12,7}, 0,16, {}, MOB_LEG_A, 32.0f, 0.5f);
        part({-2,0,-2}, {4,6,4}, { 3,12,7}, 0,16, {}, MOB_LEG_B, 32.0f, 0.5f);
        part({-2,0,-2}, {4,6,4}, {-3,12,-5}, 0,16, {}, MOB_LEG_B, 32.0f, 0.5f);
        part({-2,0,-2}, {4,6,4}, { 3,12,-5}, 0,16, {}, MOB_LEG_A, 32.0f, 0.5f);
    });
    buildQuadruped(92, 12, true, false);
    buildQuadruped(95, 8, false, false);
    buildQuadruped(96, 12, true, false);
    buildQuadruped(98, 8, false, false);
    // modelHorse (128x128). Shins hang off the legs and hooves off the shins so they follow the swing animation ids: 30/31 back-left leg/shin, 32/33 back-right, 34/35 front-left, 36/37 front-right
    buildMobMesh(100, [&](const auto& part) {
        const float t = 128.0f;
        constexpr float headTilt = -0.5235988f; // vanilla +30 degrees, inverted with the mesh
        part({-5,-8,-19},{10,10,24},{0,11,9},0,34,{},MOB_STATIC,t);
        part({-1,-1,0},{2,2,3},{0,3,14},44,0,{1.134464f,0,0},MOB_STATIC,t);
        part({-1.5f,-2,3},{3,4,7},{0,3,14},38,7,{1.134464f,0,0},MOB_STATIC,t);
        part({-1.5f,-4.5f,9},{3,4,7},{0,3,14},24,3,{1.40215f,0,0},MOB_STATIC,t);
        int nextPart = 4; // body and the three tail parts come first
        const auto leg = [&](const glm::vec3 legBox, const glm::vec3 legSize, const glm::vec3 shinBox,
                             const glm::vec3 hoofBox, const glm::vec3 pivot, const int legU, const int legV,
                             const int shinV, const int hoofV, const uint8_t legAnim) {
            const int legIndex = nextPart;
            nextPart += 3;
            part(legBox, legSize, pivot, legU, legV, {}, legAnim, t);
            const glm::vec3 shinPivot{pivot.x, pivot.y + 7.0f, pivot.z};
            part(shinBox, {3,5,3}, shinPivot, legU, shinV, {}, static_cast<uint8_t>(legAnim + 1), t, 0.0f,
                false, false, legIndex);
            part(hoofBox, {4,3,4}, shinPivot, legU, hoofV, {}, MOB_STATIC, t, 0.0f, false, false, legIndex + 1);
        };
        leg({-2.5f,-2,-2.5f},{4,9,5},{-2,0,-1.5f},{-2.5f,5.1f,-2},{4,9,11},78,29,43,51,30);
        leg({-1.5f,-2,-2.5f},{4,9,5},{-1,0,-1.5f},{-1.5f,5.1f,-2},{-4,9,11},96,29,43,51,32);
        leg({-1.9f,-1,-2.1f},{3,8,4},{-1.9f,0,-1.6f},{-2.4f,5.1f,-2.1f},{4,9,-8},44,29,41,51,34);
        leg({-1.1f,-1,-2.1f},{3,8,4},{-1.1f,0,-1.6f},{-1.6f,5.1f,-2.1f},{-4,9,-8},60,29,41,51,36);
        part({-2.5f,-10,-1.5f},{5,5,7},{0,4,-10},0,0,{headTilt,0,0},MOB_HEAD,t);
        part({-2,-10,-7},{4,3,6},{0,4,-10},24,18,{headTilt,0,0},MOB_HEAD,t);
        part({-2,-7,-6.5f},{4,2,5},{0,4,-10},24,27,{headTilt,0,0},MOB_HEAD,t);
        part({0.45f,-12,4},{2,3,1},{0,4,-10},0,0,{headTilt,0,0},MOB_HEAD,t);
        part({-2.45f,-12,4},{2,3,1},{0,4,-10},0,0,{headTilt,0,0},MOB_HEAD,t);
        part({-2.05f,-9.8f,-2},{4,14,8},{0,4,-10},0,12,{headTilt,0,0},MOB_HEAD,t);
        part({-1,-11.5f,5},{2,16,4},{0,4,-10},58,0,{headTilt,0,0},MOB_HEAD,t);
        // long donkey/mule ears replace the horse ears (parts 19/20 are hidden for them, 23/24 for horses)
        part({-2,-16,4},{2,7,1},{0,4,-10},0,12,{headTilt,0,-0.2617994f},MOB_HEAD,t);
        part({0,-16,4},{2,7,1},{0,4,-10},0,12,{headTilt,0,0.2617994f},MOB_HEAD,t);
    });
    // modelMinecart: pivots are shifted by 18 because the model's origin sits 0.375 above the entity
    // ---- block entity models (ids 300+), in vanilla model coordinates; see Renderer::renderBlockEntityModels.
    // Animated parts use ids 40 (chest lid and knob) and 41-47 (enchanting table book).
    const auto buildChest = [&](const uint16_t id, const bool large) {
        buildMobMesh(id, [&](const auto& part) {
            const float canvas = large ? 65.0f : 64.0f;
            const float length = large ? 30.0f : 14.0f;
            part({0,-5,-14},{length,5,14},{1,7,15},0,0,{},40,canvas,0.0f,false,true);
            part({-1,-2,-15},{2,4,1},{large ? 16.0f : 8.0f,7,15},0,0,{},40,canvas,0.0f,false,true);
            part({0,0,0},{length,10,14},{1,6,1},0,19,{},MOB_STATIC,canvas,0.0f,false,true);
        });
    };
    buildChest(300, false); buildChest(301, true); buildChest(302, false); buildChest(303, true); buildChest(304, false);
    buildMobMesh(305, [&](const auto& part) { // ModelBook
        part({-6,-5,0},{6,10,0},{0,0,-1},0,0,{},41,32.0f,0.0f,false,true);
        part({0,-5,0},{6,10,0},{0,0,1},16,0,{},42,32.0f,0.0f,false,true);
        part({0,-4,-0.99f},{5,8,1},{0,0,0},0,10,{},43,32.0f,0.0f,false,true);
        part({0,-4,-0.01f},{5,8,1},{0,0,0},12,10,{},44,32.0f,0.0f,false,true);
        part({0,-4,0},{5,8,0},{0,0,0},24,10,{},45,32.0f,0.0f,false,true);
        part({0,-4,0},{5,8,0},{0,0,0},24,10,{},46,32.0f,0.0f,false,true);
        part({-1,-5,0},{2,10,0},{0,0,0},12,0,{0,1.5707964f,0},MOB_STATIC,32.0f,0.0f,false,true);
    });
    const auto buildSign = [&](const uint16_t id, const bool standing) { // ModelSign
        buildMobMesh(id, [&](const auto& part) {
            part({-12,-14,-1},{24,12,2},{0,0,0},0,0,{},MOB_STATIC,32.0f,0.0f,false,true);
            if (standing) part({-1,-2,-1},{2,14,2},{0,0,0},0,14,{},MOB_STATIC,32.0f,0.0f,false,true);
        });
    };
    buildSign(306, true); buildSign(307, false);
    buildMobMesh(210, [&](const auto& part) {
        constexpr float halfPi = 1.57079632679f;
        part({-10,-8,-1},{20,16,2},{0,22,0},0,10,{-halfPi,0,0},MOB_STATIC);
        part({-9,-7,-1},{18,14,1},{0,22.1f,0},44,10,{halfPi,0,0},MOB_STATIC);
        part({-8,-9,-1},{16,8,2},{-9,22,0},0,0,{0,3 * halfPi,0},MOB_STATIC);
        part({-8,-9,-1},{16,8,2},{9,22,0},0,0,{0,halfPi,0},MOB_STATIC);
        part({-8,-9,-1},{16,8,2},{0,22,-7},0,0,{0,2 * halfPi,0},MOB_STATIC);
        part({-8,-9,-1},{16,8,2},{0,22,7},0,0,{},MOB_STATIC);
    });
    buildMobMesh(101, [&](const auto& part) {
        part({-1,5.5f,-3.7f},{2,1,7},{3,17.5f,3.7f},26,24,{},6);
        part({-1,5.5f,-3.7f},{2,1,7},{-3,17.5f,3.7f},8,24,{},6);
        part({-1,0,0},{2,4,5},{3,17.5f,3.7f},30,15,{0.34906584f,0,0},7);
        part({-1,0,0},{2,4,5},{-3,17.5f,3.7f},16,15,{0.34906584f,0,0},7);
        part({-3,-2,-10},{6,5,10},{0,19,8},0,0,{0.34906584f,0,0},MOB_STATIC);
        part({-1,0,-1},{2,7,2},{3,17,-1},8,15,{0.17453292f,0,0},8);
        part({-1,0,-1},{2,7,2},{-3,17,-1},0,15,{0.17453292f,0,0},8);
        part({-2.5f,-4,-5},{5,4,5},{0,16,-1},32,0,{},MOB_HEAD);
        part({-2.5f,-9,-1},{2,5,1},{0,16,-1},52,0,{0,-0.2617994f,0},MOB_HEAD);
        part({0.5f,-9,-1},{2,5,1},{0,16,-1},58,0,{0,0.2617994f,0},MOB_HEAD);
        part({-1.5f,-1.5f,0},{3,3,2},{0,20,7},52,6,{0.3490659f,0,0},MOB_STATIC);
        part({-0.5f,-2.5f,-5.5f},{1,1,1},{0,16,-1},32,9,{},MOB_HEAD);
    });
    buildMobMesh(93, [&](const auto& part) {
        part({-2,-6,-2},{4,6,3},{0,15,-4},0,0,{},MOB_HEAD);
        part({-2,-4,-4},{4,2,2},{0,15,-4},14,0,{},MOB_HEAD);
        part({-3,-4,-3},{6,8,6},{0,16,0},0,9,{-1.57079632679f,0,0},MOB_STATIC);
        part({-1,0,-3},{3,5,3},{-2,19,1},26,0,{},MOB_LEG_A);
        part({-1,0,-3},{3,5,3},{1,19,1},26,0,{},MOB_LEG_B);
        part({0,0,-3},{1,4,6},{-4,13,0},24,13,{},MOB_ARM_A);
        part({-1,0,-3},{1,4,6},{4,13,0},24,13,{},MOB_ARM_B);
    });
    const auto buildBiped = [&](const uint8_t type, const bool thin, const bool tall) {
        buildMobMesh(type, [&](const auto& part) {
            const float limb = thin ? 2.0f : 4.0f;
            const float length = tall ? 30.0f : 12.0f;
            part({-4,-8,-4},{8,8,8},{0,0,0},0,0,{},MOB_HEAD);
            part({-4,0,-2},{8,12,4},{0,0,0},16,16,{},MOB_STATIC);
            part({-limb/2,-2,-limb/2},{limb,length,limb},{-5,2,0},40,16,{},MOB_ARM_A);
            part({-limb/2,-2,-limb/2},{limb,length,limb},{5,2,0},40,16,{},MOB_ARM_B);
            part({-limb/2,0,-limb/2},{limb,length,limb},{-2,12,0},0,16,{},MOB_LEG_B);
            part({-limb/2,0,-limb/2},{limb,length,limb},{2,12,0},0,16,{},MOB_LEG_A);
        });
    };
    for (const uint8_t type : {uint8_t(51),uint8_t(53),uint8_t(54),uint8_t(57),uint8_t(66),uint8_t(97),uint8_t(99),uint8_t(120)})
        buildBiped(type, type == 51, false);
    // modelEnderman: ModelBiped(0, -14) with 30px limbs
    buildMobMesh(58, [&](const auto& part) {
        part({-4,-8,-4},{8,8,8},{0,-14,0},0,0,{},MOB_HEAD);
        // headwear layer: the lower jaw, inflated by -0.5
        part({-4,-8,-4},{8,8,8},{0,-14,0},0,16,{},MOB_HEAD, 32.0f, -0.5f);
        part({-4,0,-2},{8,12,4},{0,-14,0},32,16,{},MOB_STATIC);
        // the source's -3/+5 shoulder pivots put one arm inside the leg; use symmetric shoulders
        part({-1,-2,-1},{2,30,2},{-5,-12,0},56,0,{},MOB_ARM_A);
        part({-1,-2,-1},{2,30,2},{5,-12,0},56,0,{},MOB_ARM_B, 32.0f, 0.0f, true);
        part({-1,0,-1},{2,30,2},{-2,-2,0},56,0,{},MOB_LEG_B);
        part({-1,0,-1},{2,30,2},{2,-2,0},56,0,{},MOB_LEG_A, 32.0f, 0.0f, true);
    });
    buildMobMesh(50, [&](const auto& part) {
        part({-4,-8,-4},{8,8,8},{0,6,0},0,0,{},MOB_HEAD);
        part({-4,0,-2},{8,12,4},{0,6,0},16,16,{},MOB_STATIC);
        part({-2,0,-2},{4,6,4},{-2,18,4},0,16,{},MOB_LEG_A);
        part({-2,0,-2},{4,6,4},{2,18,4},0,16,{},MOB_LEG_B);
        part({-2,0,-2},{4,6,4},{-2,18,-4},0,16,{},MOB_LEG_B);
        part({-2,0,-2},{4,6,4},{2,18,-4},0,16,{},MOB_LEG_A);
    });
    // modelSlime: inner body, eyes and mouth, then the translucent outer body
    buildMobMesh(55, [&](const auto& part) {
        part({-3,17,-3}, {6,6,6}, {0,0,0}, 0,16, {}, MOB_STATIC);
        part({-3.25f,18,-3.5f}, {2,2,2}, {0,0,0}, 32,0, {}, MOB_STATIC);
        part({1.25f,18,-3.5f}, {2,2,2}, {0,0,0}, 32,4, {}, MOB_STATIC);
        part({0,21,-3.5f}, {1,1,1}, {0,0,0}, 32,8, {}, MOB_STATIC);
        part({-4,16,-4}, {8,8,8}, {0,0,0}, 0,0, {}, MOB_STATIC);
    });
    for (const uint8_t type : {uint8_t(62),uint8_t(67),uint8_t(68)}) {
        buildMobMesh(type, [&](const auto& part) {
            part({-4,-8,-4},{8,8,8},{0,16,0},0,0,{},MOB_HEAD);
        });
    }
    for (const uint8_t type : {uint8_t(52), uint8_t(59)}) {
        // modelSpider; legs are animated in the renderer (ids 20-27, vanilla legs 1-8)
        buildMobMesh(type, [&](const auto& part) {
            part({-4,-4,-8},{8,8,8},{0,15,-3},32,4,{},MOB_HEAD);
            part({-3,-3,-3},{6,6,6},{0,15,0},0,0,{},MOB_STATIC);
            part({-5,-4,-6},{10,8,12},{0,15,9},0,12,{},MOB_STATIC);
            for (int leg = 0; leg < 8; ++leg) {
                const bool left = (leg & 1) == 0; // legs 1,3,5,7 sit at x = -4
                part(left ? glm::vec3{-15,-1,-1} : glm::vec3{-1,-1,-1}, {16,2,2},
                    {left ? -4.0f : 4.0f, 15.0f, 2.0f - static_cast<float>(leg / 2)}, 18,0, {},
                    static_cast<uint8_t>(20 + leg));
            }
        });
    }
    buildMobMesh(94, [&](const auto& part) {
        part({-6,-8,-6},{12,16,12},{0,8,0},0,0,{},MOB_STATIC);
        for (int tentacle = 0; tentacle < 8; ++tentacle) {
            const float angle = static_cast<float>(tentacle) * 0.78539816339f;
            part({-1,0,-1},{2,18,2},{std::cos(angle)*5.0f,16.0f,std::sin(angle)*5.0f},
                48,0,{},(tentacle & 1) ? MOB_LEG_A : MOB_LEG_B);
        }
    });
    buildMobMesh(56, [&](const auto& part) {
        part({-8,-8,-8},{16,16,16},{0,8,0},0,0,{},MOB_STATIC);
        for (int x = -1; x <= 1; ++x) for (int z = -1; z <= 1; ++z)
            part({-1,0,-1},{2,10,2},{x*5.0f,16.0f,z*5.0f},0,0,{},MOB_LEG_A);
    });
    buildMobMesh(60, [&](const auto& part) {
        part({-2,-2,-5},{4,4,10},{0,20,0},0,0,{},MOB_STATIC);
        part({-4,-1,-1},{8,2,2},{0,20,1},0,16,{},MOB_STATIC);
    });
    buildMobMesh(61, [&](const auto& part) {
        part({-4,-4,-4},{8,8,8},{0,12,0},0,0,{},MOB_HEAD);
        for (int rod = 0; rod < 8; ++rod) {
            const float angle = static_cast<float>(rod) * 0.78539816339f;
            part({-1,-4,-1},{2,8,2},{std::cos(angle)*6.0f,14.0f,std::sin(angle)*6.0f},
                0,16,{},MOB_STATIC);
        }
    });
    buildMobMesh(65, [&](const auto& part) {
        part({-3,-3,-3},{6,6,6},{0,16,0},0,0,{},MOB_HEAD);
        part({-3,-4,-2},{6,8,4},{0,18,0},0,16,{},MOB_STATIC);
        part({-10,0,-1},{10,1,6},{-3,15,0},24,0,{},MOB_ARM_A);
        part({0,0,-1},{10,1,6},{3,15,0},24,0,{},MOB_ARM_B);
    });

    updateProjection(fbWidth, fbHeight, 90.0f);

    constexpr int atlasSize = atlasTilesPerRow * atlasTileSize;
    glGenTextures(1, &blockAtlasTexture);
    glBindTexture(GL_TEXTURE_2D, blockAtlasTexture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlasSize, atlasSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, blockAtlas.first.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 4);
    std::vector<unsigned char> mipmap = blockAtlas.first;
    int mipmapSize = atlasSize;
    for (int level = 1; level <= 4; ++level) {
        mipmap = generateMipmapLevel(mipmap, mipmapSize, mipmapSize, true);
        mipmapSize /= 2;
        glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, mipmapSize, mipmapSize, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, mipmap.data());
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    animatedAtlas = std::make_unique<AnimatedAtlas>(blockAtlas.second);
    glBindTexture(GL_TEXTURE_2D, 0);

    grassColorTexture = loadTexture("assets/minecraft/textures/colormap/grass.png");
    foliageColorTexture = loadTexture("assets/minecraft/textures/colormap/foliage.png");
    playerSkinTexture = loadTexture("assets/minecraft/textures/entity/steve.png");
    alexSkinTexture = loadTexture("assets/minecraft/textures/entity/alex.png");
    entityShadowTexture = loadTexture("assets/minecraft/textures/misc/shadow.png");
    itemAtlasTexture = loadTexture("assets/itematlas.png");
    itemGlintTexture = loadTexture("assets/minecraft/textures/misc/enchanted_item_glint.png");
    if (unsigned char* itemAtlasData = stbi_load("assets/itematlas.png",
            &itemAtlasPixelWidth, &itemAtlasPixelHeight, &itemAtlasPixelChannels, 4)) {
        itemAtlasPixelChannels = 4;
        itemAtlasPixels.assign(itemAtlasData,
            itemAtlasData + static_cast<size_t>(itemAtlasPixelWidth) * itemAtlasPixelHeight * 4);
        stbi_image_free(itemAtlasData);
    }
    skeletonSkullTexture = loadTexture("assets/minecraft/textures/entity/skeleton/skeleton.png");
    witherSkullTexture = loadTexture("assets/minecraft/textures/entity/skeleton/wither_skeleton.png");
    zombieSkullTexture = loadTexture("assets/minecraft/textures/entity/zombie/zombie.png");
    creeperSkullTexture = loadTexture("assets/minecraft/textures/entity/creeper/creeper.png");
    const auto setMobTexture = [&](const uint16_t type, const char* path) {
        if (const auto found = mobMeshes.find(type); found != mobMeshes.end())
            found->second.texture = loadTexture(path);
    };
    setMobTexture(50, "assets/minecraft/textures/entity/creeper/creeper.png");
    setMobTexture(30, "assets/minecraft/textures/entity/armorstand/wood.png");
    setMobTexture(51, "assets/minecraft/textures/entity/skeleton/skeleton.png");
    setMobTexture(52, "assets/minecraft/textures/entity/spider/spider.png");
    setMobTexture(53, "assets/minecraft/textures/entity/zombie/zombie.png");
    setMobTexture(54, "assets/minecraft/textures/entity/zombie/zombie.png");
    setMobTexture(55, "assets/minecraft/textures/entity/slime/slime.png");
    setMobTexture(56, "assets/minecraft/textures/entity/ghast/ghast.png");
    setMobTexture(57, "assets/minecraft/textures/entity/zombie_pigman.png");
    setMobTexture(58, "assets/minecraft/textures/entity/enderman/enderman.png");
    setMobTexture(59, "assets/minecraft/textures/entity/spider/cave_spider.png");
    setMobTexture(60, "assets/minecraft/textures/entity/silverfish.png");
    setMobTexture(61, "assets/minecraft/textures/entity/blaze.png");
    setMobTexture(62, "assets/minecraft/textures/entity/slime/magmacube.png");
    setMobTexture(65, "assets/minecraft/textures/entity/bat.png");
    setMobTexture(66, "assets/minecraft/textures/entity/witch.png");
    setMobTexture(67, "assets/minecraft/textures/entity/endermite.png");
    setMobTexture(68, "assets/minecraft/textures/entity/guardian.png");
    setMobTexture(90, "assets/minecraft/textures/entity/pig/pig.png");
    setMobTexture(91, "assets/minecraft/textures/entity/sheep/sheep.png");
    setMobTexture(250, "assets/minecraft/textures/entity/sheep/sheep_fur.png");
    setMobTexture(92, "assets/minecraft/textures/entity/cow/cow.png");
    setMobTexture(93, "assets/minecraft/textures/entity/chicken.png");
    setMobTexture(94, "assets/minecraft/textures/entity/squid.png");
    setMobTexture(95, "assets/minecraft/textures/entity/wolf/wolf.png");
    setMobTexture(96, "assets/minecraft/textures/entity/cow/mooshroom.png");
    setMobTexture(97, "assets/minecraft/textures/entity/snowman.png");
    setMobTexture(98, "assets/minecraft/textures/entity/cat/ocelot.png");
    setMobTexture(99, "assets/minecraft/textures/entity/iron_golem.png");
    setMobTexture(100, "assets/minecraft/textures/entity/horse/horse_white.png");
    setMobTexture(210, "assets/minecraft/textures/entity/minecart.png");
    setMobTexture(300, "assets/minecraft/textures/entity/chest/normal.png");
    setMobTexture(301, "assets/minecraft/textures/entity/chest/normal_double.png");
    setMobTexture(302, "assets/minecraft/textures/entity/chest/trapped.png");
    setMobTexture(303, "assets/minecraft/textures/entity/chest/trapped_double.png");
    setMobTexture(304, "assets/minecraft/textures/entity/chest/ender.png");
    setMobTexture(305, "assets/minecraft/textures/entity/enchanting_table_book.png");
    setMobTexture(306, "assets/minecraft/textures/entity/sign.png");
    setMobTexture(307, "assets/minecraft/textures/entity/sign.png");
    static constexpr const char* coats[7]{"white", "creamy", "chestnut", "brown", "black", "gray", "darkbrown"};
    for (int index = 0; index < 7; ++index)
        horseCoatTextures[index] = loadTexture((std::string("assets/minecraft/textures/entity/horse/horse_")
            + coats[index] + ".png").c_str());
    static constexpr const char* markings[5]{nullptr, "white", "whitefield", "whitedots", "blackdots"};
    for (int index = 1; index < 5; ++index)
        horseMarkingTextures[index] = loadTexture((std::string("assets/minecraft/textures/entity/horse/horse_markings_")
            + markings[index] + ".png").c_str());
    donkeyTexture = loadTexture("assets/minecraft/textures/entity/horse/donkey.png");
    muleTexture = loadTexture("assets/minecraft/textures/entity/horse/mule.png");
    zombieHorseTexture = loadTexture("assets/minecraft/textures/entity/horse/horse_zombie.png");
    skeletonHorseTexture = loadTexture("assets/minecraft/textures/entity/horse/horse_skeleton.png");
    setMobTexture(101, "assets/minecraft/textures/entity/rabbit/brown.png");
    setMobTexture(120, "assets/minecraft/textures/entity/villager/villager.png");
    glBindTexture(GL_TEXTURE_2D, itemAtlasTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glBindTexture(GL_TEXTURE_2D, entityShadowTexture);
    constexpr float transparentBorder[4]{0.0f, 0.0f, 0.0f, 0.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, transparentBorder);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    handShader->use();
    handShader->setInt("playerSkin", 0);

    blockShader->use();
    blockShader->setInt("texture1", 0);
    blockShader->setInt("grassColorMap", 1);
    blockShader->setInt("foliageColorMap", 2);
    initSky();
}

void Renderer::initSky() {
    skyShader = new Shader("assets/shaders/sky.vert", "assets/shaders/sky.frag");
    celestialShader = new Shader("assets/shaders/celestial.vert", "assets/shaders/celestial.frag");
    starShader = new Shader("assets/shaders/stars.vert", "assets/shaders/stars.frag");
    sunTexture = loadTexture("assets/minecraft/textures/environment/sun.png");
    moonTexture = loadTexture("assets/minecraft/textures/environment/moon_phases.png");

    constexpr float celestialVertices[] = {
        -30.0f, 100.0f, -30.0f, 0.0f, 0.0f,
         30.0f, 100.0f, -30.0f, 1.0f, 0.0f,
         30.0f, 100.0f,  30.0f, 1.0f, 1.0f,
        -30.0f, 100.0f, -30.0f, 0.0f, 0.0f,
         30.0f, 100.0f,  30.0f, 1.0f, 1.0f,
        -30.0f, 100.0f,  30.0f, 0.0f, 1.0f
    };
    glGenVertexArrays(1, &celestialVAO);
    glGenBuffers(1, &celestialVBO);
    glBindVertexArray(celestialVAO);
    glBindBuffer(GL_ARRAY_BUFFER, celestialVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(celestialVertices), celestialVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    std::mt19937 random(10842);
    std::uniform_real_distribution<float> coordinate(-1.0f, 1.0f);
    std::vector<glm::vec3> stars;
    stars.reserve(1500);
    while (stars.size() < 1500) {
        glm::vec3 direction{coordinate(random), coordinate(random), coordinate(random)};
        const float lengthSquared = glm::dot(direction, direction);
        if (lengthSquared >= 1.0f || lengthSquared <= 0.01f) continue;
        stars.push_back(glm::normalize(direction) * 100.0f);
    }
    starCount = static_cast<GLsizei>(stars.size());
    glGenVertexArrays(1, &starVAO);
    glGenBuffers(1, &starVBO);
    glBindVertexArray(starVAO);
    glBindBuffer(GL_ARRAY_BUFFER, starVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(stars.size() * sizeof(glm::vec3)), stars.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    celestialShader->use();
    celestialShader->setInt("celestialTexture", 0);
}

void Renderer::renderSky(const World* world, const float partialTicks) const {
    ZoneScopedN("Sky render");
    const float angle = world->celestialAngle(partialTicks);
    const float dayFactor = std::clamp(std::cos(angle * static_cast<float>(M_PI) * 2.0f) * 2.0f + 0.5f, 0.0f, 1.0f);
    float temperature = biomeTemperature(world->getBiomeAt(
        static_cast<int>(std::floor(mc->player->position.x)),
        static_cast<int>(std::floor(mc->player->position.z)))) / 3.0f;
    temperature = std::clamp(temperature, -1.0f, 1.0f);
    const glm::vec3 skyColor = hsvToRgb(0.62222224f - temperature * 0.05f,
        0.5f + temperature * 0.1f, 1.0f) * dayFactor;
    const glm::mat4 skyView = glm::mat4(glm::mat3(view));

    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    skyShader->use();
    skyShader->setMat4("projection", glm::value_ptr(projection));
    skyShader->setMat4("view", glm::value_ptr(skyView));
    skyShader->setVec3("horizonColor", skyColor);
    skyShader->setVec3("zenithColor", skyColor);
    glBindVertexArray(cubeVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);

    float starBrightness = 1.0f - (std::cos(angle * static_cast<float>(M_PI) * 2.0f) * 2.0f + 0.25f);
    starBrightness = std::clamp(starBrightness, 0.0f, 1.0f);
    starBrightness = starBrightness * starBrightness * 0.5f;
    if (starBrightness > 0.0f) {
        starShader->use();
        starShader->setMat4("projection", glm::value_ptr(projection));
        starShader->setMat4("view", glm::value_ptr(skyView));
        starShader->setFloat("brightness", starBrightness);
        glBindVertexArray(starVAO);
        glDrawArrays(GL_POINTS, 0, starCount);
    }

    celestialShader->use();
    celestialShader->setMat4("projection", glm::value_ptr(projection));
    celestialShader->setMat4("view", glm::value_ptr(skyView));
    celestialShader->setVec4("color", {1.0f, 1.0f, 1.0f, 1.0f});
    glBindVertexArray(celestialVAO);
    glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(angle * 360.0f), glm::vec3{1.0f, 0.0f, 0.0f});
    celestialShader->setMat4("model", glm::value_ptr(model));
    celestialShader->setVec2("uvOffset", 0.0f, 0.0f);
    celestialShader->setVec2("uvScale", 1.0f, 1.0f);
    celestialShader->setInt("monochrome", 1);
    glBindTexture(GL_TEXTURE_2D, sunTexture);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    model = glm::rotate(model, glm::radians(180.0f), glm::vec3{1.0f, 0.0f, 0.0f});
    celestialShader->setMat4("model", glm::value_ptr(model));
    const int moonPhase = static_cast<int>(world->worldTime.load(std::memory_order_relaxed) / 24000 % 8);
    celestialShader->setVec2("uvOffset", static_cast<float>(moonPhase % 4) * 0.25f,
        static_cast<float>(moonPhase / 4) * 0.5f);
    celestialShader->setVec2("uvScale", 0.25f, 0.5f);
    celestialShader->setInt("monochrome", 0);
    glBindTexture(GL_TEXTURE_2D, moonTexture);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}

void Renderer::renderBlockDamage(const World* world) const {
    std::vector<World::DestroyBlockProgress> damagedBlocks;
    {
        std::lock_guard lock(world->blockEventMutex);
        damagedBlocks.reserve(world->damagedBlocks.size());
        for (const auto& [breakerId, damage] : world->damagedBlocks) damagedBlocks.push_back(damage);
    }
    if (damagedBlocks.empty()) return;

    damageShader->use();
    damageShader->setMat4("projection", glm::value_ptr(projection));
    damageShader->setMat4("view", glm::value_ptr(view));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, blockAtlasTexture);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_DST_COLOR, GL_SRC_COLOR, GL_ONE, GL_ZERO);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-3.0f, -3.0f);
    glEnable(GL_CULL_FACE);
    glBindVertexArray(damageVAO);

    for (const auto& damage : damagedBlocks) {
        const Block block = world->getBlockAt(damage.position.x, damage.position.y, damage.position.z);
        if (blockId(block) == blockId(BLOCK_AIR)) continue;
        const std::string texture = "destroy_stage_" + std::to_string(std::min<uint8_t>(damage.progress, 9));
        const char* textureKey = BlockUtil::getAtlasTexturePtr(blockAtlas.second, texture);
        const auto& [tileX, tileY] = blockAtlas.second.blockAtlasPos.at(textureKey);
        damageShader->setVec2("tileOrigin", static_cast<float>(tileX) / atlasTilesPerRow,
            static_cast<float>(tileY) / atlasTilesPerRow);

        const auto drawBox = [&](const AABB& bounds,
                                 const std::array<bool, 6>& faces = {true, true, true, true, true, true}) {
            constexpr float expansion = 0.001f;
            const glm::vec3 minimum(bounds.minX, bounds.minY, bounds.minZ);
            const glm::vec3 size(bounds.maxX - bounds.minX, bounds.maxY - bounds.minY,
                bounds.maxZ - bounds.minZ);
            damageShader->setVec3("blockMinimum", glm::vec3(
                static_cast<float>(bounds.minX - damage.position.x),
                static_cast<float>(bounds.minY - damage.position.y),
                static_cast<float>(bounds.minZ - damage.position.z)));
            damageShader->setVec3("blockMaximum", glm::vec3(
                static_cast<float>(bounds.maxX - damage.position.x),
                static_cast<float>(bounds.maxY - damage.position.y),
                static_cast<float>(bounds.maxZ - damage.position.z)));
            glm::mat4 model = glm::translate(glm::mat4(1.0f),
                minimum + size * 0.5f - cameraPos);
            model = glm::scale(model, size + glm::vec3(expansion * 2.0f));
            damageShader->setMat4("model", glm::value_ptr(model));
            for (int face = 0; face < 6; ++face) {
                if (faces[face]) glDrawArrays(GL_TRIANGLES, face * 6, 6);
            }
        };

        switch (blockId(block)) {
            case blockId(BLOCK_TALLGRASS):
            case blockId(BLOCK_DOUBLE_PLANT):
            case blockId(BLOCK_DEADBUSH):
            case blockId(BLOCK_YELLOW_FLOWER):
            case blockId(BLOCK_RED_FLOWER):
            case blockId(BLOCK_BROWN_MUSHROOM):
            case blockId(BLOCK_RED_MUSHROOM):
            case blockId(BLOCK_SAPLING): {
                uint64_t random = static_cast<uint64_t>(static_cast<int64_t>(damage.position.x) * 3129871LL)
                    ^ static_cast<uint64_t>(static_cast<int64_t>(damage.position.z) * 116129781LL);
                random = random * random * 42317861ULL + random * 11ULL;
                const float offsetX = (static_cast<float>((random >> 16U) & 15U) / 15.0f - 0.5f) * 0.5f;
                const float offsetZ = (static_cast<float>((random >> 24U) & 15U) / 15.0f - 0.5f) * 0.5f;
                const bool offsetY = blockId(block) != blockId(BLOCK_DOUBLE_PLANT);
                const float verticalOffset = offsetY
                    ? (static_cast<float>((random >> 20U) & 15U) / 15.0f - 1.0f) * 0.2f : 0.0f;
                const glm::vec3 center{damage.position.x + 0.5f + offsetX,
                    damage.position.y + 0.5f + verticalOffset,
                    damage.position.z + 0.5f + offsetZ};
                glm::mat4 model = glm::translate(glm::mat4(1.0f), center - cameraPos);
                model = glm::scale(model, glm::vec3(1.002f));
                damageShader->setMat4("model", glm::value_ptr(model));
                glDrawArrays(GL_TRIANGLES, 36, 12);
                break;
            }
            case blockId(BLOCK_SNOW_LAYER): {
                const double height = static_cast<double>((blockMeta(block) & 7) + 1) / 8.0;
                drawBox({static_cast<double>(damage.position.x), static_cast<double>(damage.position.y),
                    static_cast<double>(damage.position.z), damage.position.x + 1.0,
                    damage.position.y + height, damage.position.z + 1.0});
                break;
            }
            case blockId(BLOCK_STONE_BUTTON): case blockId(BLOCK_WOODEN_BUTTON): {
                const double x = damage.position.x, y = damage.position.y, z = damage.position.z;
                const double depth = (blockMeta(block) & 8) != 0 ? 1.0 / 16.0 : 2.0 / 16.0;
                switch (blockMeta(block) & 7) {
                    case 1: drawBox({x, y + 0.375, z + 0.3125, x + depth, y + 0.625, z + 0.6875}); break;
                    case 2: drawBox({x + 1.0 - depth, y + 0.375, z + 0.3125, x + 1.0, y + 0.625, z + 0.6875}); break;
                    case 3: drawBox({x + 0.3125, y + 0.375, z, x + 0.6875, y + 0.625, z + depth}); break;
                    case 4: drawBox({x + 0.3125, y + 0.375, z + 1.0 - depth, x + 0.6875, y + 0.625, z + 1.0}); break;
                    default: break;
                }
                break;
            }
            case blockId(BLOCK_FLOWER_POT): {
                const double x = damage.position.x, y = damage.position.y, z = damage.position.z;
                drawBox({x + 5.0 / 16.0, y, z + 5.0 / 16.0,
                    x + 11.0 / 16.0, y + 6.0 / 16.0, z + 11.0 / 16.0});
                break;
            }
            case blockId(BLOCK_STANDING_SIGN): {
                const double x = damage.position.x, y = damage.position.y, z = damage.position.z;
                drawBox({x + 0.25, y, z + 0.25, x + 0.75, y + 1.0, z + 0.75});
                break;
            }
            case blockId(BLOCK_WALL_SIGN): {
                const double x = damage.position.x, y = damage.position.y, z = damage.position.z;
                switch (blockMeta(block)) {
                    case 2: drawBox({x, y + 0.28125, z + 0.875, x + 1.0, y + 0.78125, z + 1.0}); break;
                    case 3: drawBox({x, y + 0.28125, z, x + 1.0, y + 0.78125, z + 0.125}); break;
                    case 4: drawBox({x + 0.875, y + 0.28125, z, x + 1.0, y + 0.78125, z + 1.0}); break;
                    default: drawBox({x, y + 0.28125, z, x + 0.125, y + 0.78125, z + 1.0}); break;
                }
                break;
            }
            default: {
                const AABB blockBox{static_cast<double>(damage.position.x), static_cast<double>(damage.position.y),
                    static_cast<double>(damage.position.z), damage.position.x + 1.0,
                    damage.position.y + 1.0, damage.position.z + 1.0};
                if (BlockUtil::isStair(block)) {
                    const bool upsideDown = (blockMeta(block) & 4) != 0;
                    const double otherMinY = damage.position.y + (upsideDown ? 0.5 : 0.0);
                    const double otherMaxY = damage.position.y + (upsideDown ? 1.0 : 0.5);
                    AABB full = blockBox;
                    AABB other{static_cast<double>(damage.position.x), otherMinY,
                        static_cast<double>(damage.position.z),
                        damage.position.x + 1.0, otherMaxY, damage.position.z + 1.0};
                    std::array<bool, 6> fullFaces{true, true, true, true, true, true};
                    std::array<bool, 6> otherFaces{true, true, true, true, true, true};
                    switch (blockMeta(block) & 3) {
                        case 0:
                            full.minX += 0.5; other.maxX -= 0.5;
                            fullFaces[2] = false; otherFaces[3] = false;
                            break;
                        case 1:
                            full.maxX -= 0.5; other.minX += 0.5;
                            fullFaces[3] = false; otherFaces[2] = false;
                            break;
                        case 2:
                            full.minZ += 0.5; other.maxZ -= 0.5;
                            fullFaces[1] = false; otherFaces[0] = false;
                            break;
                        case 3:
                            full.maxZ -= 0.5; other.minZ += 0.5;
                            fullFaces[0] = false; otherFaces[1] = false;
                            break;
                        default: break;
                    }
                    drawBox(full, fullFaces);
                    drawBox(other, otherFaces);
                    break;
                }
                if (BlockUtil::isPane(block)) {
                    const auto connects = [&](const int x, const int z) {
                        const Block neighbor = world->getBlockAt(x, damage.position.y, z);
                        return BlockUtil::canPaneConnectTo(neighbor);
                    };
                    bool north = connects(damage.position.x, damage.position.z - 1);
                    bool south = connects(damage.position.x, damage.position.z + 1);
                    bool west = connects(damage.position.x - 1, damage.position.z);
                    bool east = connects(damage.position.x + 1, damage.position.z);
                    if (!north && !south && !west && !east) north = south = west = east = true;
                    const double x = damage.position.x;
                    const double y = damage.position.y;
                    const double z = damage.position.z;
                    drawBox({x + 7.0 / 16.0, y, z + 7.0 / 16.0,
                        x + 9.0 / 16.0, y + 1.0, z + 9.0 / 16.0});
                    if (north) drawBox({x + 7.0 / 16.0, y, z, x + 9.0 / 16.0, y + 1.0, z + 0.5});
                    if (south) drawBox({x + 7.0 / 16.0, y, z + 0.5, x + 9.0 / 16.0, y + 1.0, z + 1.0});
                    if (west) drawBox({x, y, z + 7.0 / 16.0, x + 0.5, y + 1.0, z + 9.0 / 16.0});
                    if (east) drawBox({x + 0.5, y, z + 7.0 / 16.0, x + 1.0, y + 1.0, z + 9.0 / 16.0});
                    break;
                }
                if (BlockUtil::isFenceGate(block)) {
                    const double x = damage.position.x, y = damage.position.y, z = damage.position.z;
                    if ((blockMeta(block) & 1) == 0)
                        drawBox({x, y, z + 6.0 / 16.0, x + 1.0, y + 1.0, z + 10.0 / 16.0});
                    else
                        drawBox({x + 6.0 / 16.0, y, z, x + 10.0 / 16.0, y + 1.0, z + 1.0});
                    break;
                }
                const std::vector<LocalBlock> parts = world->getCollidingBlocks(blockBox);
                bool drewPart = false;
                for (const LocalBlock& part : parts) {
                    if (part.x != damage.position.x || part.y != damage.position.y
                        || part.z != damage.position.z) continue;
                    drawBox(part.bounds);
                    drewPart = true;
                }
                if (!drewPart) drawBox(blockBox);
                break;
            }
        }
    }

    glBindVertexArray(0);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::renderSelectionBox() const {
    const MovingObjectPosition& mouseOver = mc->objectMouseOver;
    if (!mouseOver.valid || mouseOver.typeOfHit != MovingObjectPosition::BLOCK) return;

    constexpr float expansion = 0.002f;
    selectionShader->use();
    selectionShader->setMat4("projection", glm::value_ptr(projection));
    selectionShader->setMat4("view", glm::value_ptr(view));
    selectionShader->setVec4("color", {0.0f, 0.0f, 0.0f, 0.4f});

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glLineWidth(2.0f);
    const Block selectedBlock = mc->world->getBlockAt(mouseOver.blockPos.x,
        mouseOver.blockPos.y, mouseOver.blockPos.z);
    if (BlockUtil::isStair(selectedBlock)) {
        const bool upsideDown = (blockMeta(selectedBlock) & 4) != 0;
        const int direction = blockMeta(selectedBlock) & 3;
        const std::array<glm::vec2, 6> normalProfile = upsideDown
            ? std::array<glm::vec2, 6>{{{0, 0.5f}, {0.5f, 0.5f}, {0.5f, 0}, {1, 0}, {1, 1}, {0, 1}}}
            : std::array<glm::vec2, 6>{{{0, 0}, {1, 0}, {1, 1}, {0.5f, 1}, {0.5f, 0.5f}, {0, 0.5f}}};
        std::vector<glm::vec3> lines;
        lines.reserve(36);
        const auto position = [&](const glm::vec2 profile, const float depth) {
            switch (direction) {
                case 0: return glm::vec3(profile.x, profile.y, depth);
                case 1: return glm::vec3(1.0f - profile.x, profile.y, depth);
                case 2: return glm::vec3(depth, profile.y, profile.x);
                case 3: return glm::vec3(depth, profile.y, 1.0f - profile.x);
                default: return glm::vec3(profile.x, profile.y, depth);
            }
        };
        for (const float depth : {0.0f, 1.0f}) {
            for (size_t index = 0; index < normalProfile.size(); ++index) {
                lines.push_back(position(normalProfile[index], depth));
                lines.push_back(position(normalProfile[(index + 1) % normalProfile.size()], depth));
            }
        }
        for (const glm::vec2 point : normalProfile) {
            lines.push_back(position(point, 0.0f));
            lines.push_back(position(point, 1.0f));
        }
        glBindBuffer(GL_ARRAY_BUFFER, stairSelectionVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(lines.size() * sizeof(glm::vec3)), lines.data());
        glBindVertexArray(stairSelectionVAO);
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(mouseOver.blockPos) - cameraPos);
        selectionShader->setMat4("model", glm::value_ptr(model));
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lines.size()));
    } else {
        glBindVertexArray(selectionVAO);
    for (uint8_t index = 0; index < mouseOver.blockBoundsCount; ++index) {
        const AABB& bounds = mouseOver.blockBounds[index];
        const glm::vec3 minimum(bounds.minX, bounds.minY, bounds.minZ);
        const glm::vec3 size(bounds.maxX - bounds.minX, bounds.maxY - bounds.minY,
                             bounds.maxZ - bounds.minZ);
        glm::mat4 model = glm::translate(glm::mat4(1.0f), minimum - cameraPos - glm::vec3(expansion));
        model = glm::scale(model, size + glm::vec3(expansion * 2.0f));
        selectionShader->setMat4("model", glm::value_ptr(model));
        glDrawArrays(GL_LINES, 0, 24);
    }
    }
    glBindVertexArray(0);
    glLineWidth(1.0f);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::setupFog(const World* world, const float partialTicks) const {
    const float angle = world->celestialAngle(partialTicks);
    const float dayFactor = std::clamp(std::cos(angle * static_cast<float>(M_PI) * 2.0f) * 2.0f + 0.5f,
        0.0f, 1.0f);
    const int renderDistance = mc->settings->getRenderDistance();
    const float blend = 1.0f - std::pow(0.25f + 0.75f * renderDistance / 32.0f, 0.25f);

    float temperature = biomeTemperature(world->getBiomeAt(
        static_cast<int>(std::floor(mc->player->position.x)),
        static_cast<int>(std::floor(mc->player->position.z)))) / 3.0f;
    temperature = std::clamp(temperature, -1.0f, 1.0f);
    const glm::vec3 skyColor = hsvToRgb(0.62222224f - temperature * 0.05f,
        0.5f + temperature * 0.1f, 1.0f) * dayFactor;
    glm::vec3 fogColor{
        0.7529412f * (dayFactor * 0.94f + 0.06f),
        0.84705883f * (dayFactor * 0.94f + 0.06f),
        dayFactor * 0.91f + 0.09f,
    };
    fogColor += (skyColor - fogColor) * blend;

    const glm::dvec3 eyes = mc->player->getPosEyes(partialTicks);
    const Block eyeBlock = world->getBlockAt(static_cast<int>(std::floor(eyes.x)),
        static_cast<int>(std::floor(eyes.y)), static_cast<int>(std::floor(eyes.z)));
    const uint16_t eyeBlockId = blockId(eyeBlock);
    int fogMode = 0;
    float fogDensity = 0.0f;
    const float farPlane = static_cast<float>(renderDistance * CHUNK_SIZE);
    const float fogStart = farPlane * 0.75f;
    const float fogEnd = farPlane;
    switch (eyeBlockId) {
        case blockId(BLOCK_WATER):
        case blockId(BLOCK_FLOWING_WATER):
            fogMode = 1;
            fogDensity = 0.1f;
            fogColor = {0.02f, 0.02f, 0.2f};
            break;
        case blockId(BLOCK_LAVA):
        case blockId(BLOCK_FLOWING_LAVA):
            fogMode = 1;
            fogDensity = 2.0f;
            fogColor = {0.6f, 0.1f, 0.0f};
            break;
        default:
            break;
    }

    blockShader->use();
    blockShader->setVec3("fogColor", fogColor);
    blockShader->setFloat("fogStart", fogStart);
    blockShader->setFloat("fogEnd", fogEnd);
    blockShader->setFloat("fogDensity", fogDensity);
    blockShader->setInt("fogMode", fogMode);

    particleShader->use();
    particleShader->setVec3("fogColor", fogColor);
    particleShader->setFloat("fogStart", fogStart);
    particleShader->setFloat("fogEnd", fogEnd);
    particleShader->setFloat("fogDensity", fogDensity);
    particleShader->setInt("fogMode", fogMode);
    blockShader->use();
}
