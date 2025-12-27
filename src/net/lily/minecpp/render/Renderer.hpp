#pragma once
#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Camera.hpp"
#include "../world/World.hpp"
#include "Shader.hpp"

class Renderer {
public:
    std::unordered_map<BlockType, unsigned int> blockTextureMap;

    unsigned int cubeVAO{}, cubeVBO{};
    Shader* blockShader{};

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
