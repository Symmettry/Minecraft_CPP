#include "Renderer.hpp"
#include <cmath>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "BlockAtlas.hpp"
#include "net/lily/minecpp/Minecraft.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "lib/stb_image.h"

Renderer::~Renderer() {
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

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "Failed to initialize GLAD\n";
        exit(EXIT_FAILURE);
    }

    glfwSetWindowUserPointer(window, this);

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int fbWidth, int fbHeight) {
        glViewport(0, 0, fbWidth, fbHeight);
        if (const auto* renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(window))) {
            renderer->updateProjection(fbWidth, fbHeight);
        }
    });

    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    glEnable(GL_DEPTH_TEST);

    blockShader = new Shader("assets/shaders/basic.vert", "assets/shaders/basic.frag");

    modelLoc = glGetUniformLocation(blockShader->ID, "model");

    // Cube vertices with texture coordinates
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

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    updateProjection(fbWidth, fbHeight);

    const auto atlasPixels = blockAtlas->createAtlas("assets/minecraft/textures/blocks/");

    constexpr int atlasSize = atlasTilesPerRow * atlasTileSize;
    glGenTextures(1, &blockAtlasTexture);
    glBindTexture(GL_TEXTURE_2D, blockAtlasTexture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlasSize, atlasSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlasPixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Renderer::processInput() const {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

bool Renderer::shouldClose() const {
    return glfwWindowShouldClose(window);
}

void Renderer::updateProjection(const int fbWidth, const int fbHeight) const {
    const float aspect = static_cast<float>(fbWidth) / static_cast<float>(fbHeight);
    const glm::mat4 projection = glm::perspective(glm::radians(90.0f), aspect, 0.1f, 100.0f);

    blockShader->use();
    blockShader->setMat4("projection", glm::value_ptr(projection));
}

unsigned int Renderer::loadTexture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);

    if (!data) {
        std::cerr << "Failed to load texture: " << path << "\n";
        return 0;
    }

    GLint internalFormat, dataFormat;
    if (nrChannels == 1) internalFormat = dataFormat = GL_RED;
    else if (nrChannels == 3) internalFormat = dataFormat = GL_RGB;
    else if (nrChannels == 4) internalFormat = dataFormat = GL_RGBA;
    else {
        stbi_image_free(data);
        std::cerr << "Unsupported channel count: " << nrChannels << "\n";
        return 0;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    return textureID;
}

void Renderer::render(const World* world) const {
    processInput();

    glClearColor(0.5f, 0.7f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    blockShader->use();
    glActiveTexture(GL_TEXTURE0);
    blockShader->setInt("texture1", 0);

    const float partialTicks = mc->timer->partialTicks;
    const glm::vec3 cameraPos(
        camera->getX(partialTicks),
        camera->getY(partialTicks),
        camera->getZ(partialTicks)
    );

    const float yaw   = glm::radians(camera->getRotY(partialTicks));
    const float pitch = glm::radians(std::clamp<float>(camera->getRotX(partialTicks), -89.95f, 89.95f));

    glm::vec3 front;
    front.x = std::cos(yaw) * std::cos(pitch);
    front.y = std::sin(pitch);
    front.z = std::sin(yaw) * std::cos(pitch);
    front = glm::normalize(front);

    const glm::mat4 view = glm::lookAt(cameraPos, cameraPos + front, glm::vec3(0.0f, 1.0f, 0.0f));
    blockShader->setMat4("view", glm::value_ptr(view));

    glBindVertexArray(cubeVAO);

    glBindTexture(GL_TEXTURE_2D, blockAtlasTexture);
    for (const auto& [fst, snd] : world->chunks) {
        const glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(fst.x * CHUNK_SIZE, 0, fst.z * CHUNK_SIZE));
        blockShader->setMat4("model", glm::value_ptr(model));
        snd.draw();
    }

    glBindVertexArray(0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    const Shader* fontShader = mc->fontRenderer->shader;
    fontShader->use();
    fontShader->setInt("text", 0);

    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    const glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(fbWidth), static_cast<float>(fbHeight), 0.0f);
    fontShader->setMat4("projection", glm::value_ptr(proj));

    mc->fontRenderer->renderText("Hello 1.8.9!", 50, 50, 2.0f, 0xFFFFFFFF, true);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glfwSwapBuffers(window);
    glfwPollEvents();
}
