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

    BlockAtlas* blockAtlas = new BlockAtlas();

    unsigned int cubeVAO{}, cubeVBO{};

    Renderer(Minecraft* mc, Camera* camera, const int w, const int h) : window(nullptr), camera(camera), mc(mc), width(w), height(h) {}
    ~Renderer();

    void init();
    void render(const World *world) const;
    [[nodiscard]] bool shouldClose() const;

    void updateProjection(int fbWidth, int fbHeight) const;

    static unsigned int loadTexture(const char *path);

    GLFWwindow* window;

private:
    Camera* camera;
    Minecraft* mc;
    int width, height;
    int modelLoc = 0;
    void processInput() const;
};
