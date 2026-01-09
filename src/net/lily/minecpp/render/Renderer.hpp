#pragma once
#define GLFW_INCLUDE_NONE
#include <glad/glad.hpp>
#include <GLFW/glfw3.h>

#include "BlockAtlas.hpp"
#include "Camera.hpp"
#include "../world/World.hpp"
#include "Shader.hpp"
#include "net/lily/minecpp/util/FrustumPlanes.hpp"

class Renderer {
public:
    GLuint blockAtlasTexture{}; // single atlas texture
    Shader* blockShader{};

    BlockAtlasData blockAtlas = BlockAtlas::loadAtlas("assets/blockatlas.png", "assets/blockatlas.dat.zst");

    unsigned int cubeVAO{}, cubeVBO{};
    mutable int fps{};

    Renderer(Minecraft* mc, Camera* camera, const int w, const int h) : window(nullptr), width(w), height(h), camera(camera), mc(mc) {}
    ~Renderer();

    void init();
    void render(const World *world) const;
    [[nodiscard]] bool shouldClose() const;

    void updateProjection(int fbWidth, int fbHeight, float fov) const;

    void updateVisibleChunks() const;

    void updateProjection(int fbWidth, int fbHeight) const;
    void updateProjection(float fov) const;

    void updateFrustums() const;

    void calculateView(float pt) const;

    static unsigned int loadTexture(const char *path);

    GLFWwindow* window;

    mutable int width, height;
    mutable bool shouldCalcView = true;

private:
    Camera* camera;
    Minecraft* mc;
    int modelLoc = 0;
    void processInput() const;

    mutable glm::mat4 projection = glm::mat4(0.0f),
                      view       = glm::mat4(0.0f),
                      projView   = glm::mat4(0.0f);

    mutable glm::vec3 cameraPos  = glm::vec3(0.0f);

    mutable bool freezeFrustum = false;

    mutable FrustumInfo frustumPlanes{};
};
