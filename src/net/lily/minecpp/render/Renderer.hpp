#pragma once
#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "BlockAtlas.hpp"
#include "Camera.hpp"
#include "../world/World.hpp"
#include "Shader.hpp"

class Renderer {
public:
    GLuint blockAtlasTexture{}; // single atlas texture
    Shader* blockShader{};

    BlockAtlasData blockAtlas = BlockAtlas::loadAtlas("assets/blockatlas.png", "assets/blockatlas.dat");

    unsigned int cubeVAO{}, cubeVBO{};
    mutable int fps{};

    Renderer(Minecraft* mc, Camera* camera, const int w, const int h) : window(nullptr), width(w), height(h), camera(camera), mc(mc) {}
    ~Renderer();

    void init();
    void render(const World *world) const;
    [[nodiscard]] bool shouldClose() const;

    void updateProjection(int fbWidth, int fbHeight) const;

    static unsigned int loadTexture(const char *path);

    GLFWwindow* window;

    mutable int width, height;

private:
    Camera* camera;
    Minecraft* mc;
    int modelLoc = 0;
    void processInput() const;

    mutable bool freezeFrustum = false;
    mutable glm::mat4 frozenProjView = glm::mat4(1.0f);
};
