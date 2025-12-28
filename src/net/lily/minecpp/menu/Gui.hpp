#pragma once
#define FLOAT static_cast<float>
#include "glad/glad.h"
#include "net/lily/minecpp/render/Shader.hpp"

struct Minecraft;

class Gui {
protected:
    static GLuint quadVAO;
    static GLuint quadVBO;
    static GLuint shaderProgram;
    static Shader* quadShader;

    static void initQuad();
public:

    static void init();
    static const Minecraft *mc;

    virtual ~Gui() = default;

    virtual void onOpen() {

    }
    virtual void onClose() {

    }

    virtual void update(float deltaSeconds) = 0;
    virtual void render() const;

    virtual bool handleKey(int key, int scancode, int action, int mods) = 0;
    virtual bool handleMouseButton(int button, int action, int mods) = 0;
    virtual bool handleCursorPos(double x, double y) = 0;

    [[nodiscard]] virtual bool blocksWorldUpdate() const = 0;
    [[nodiscard]] virtual bool blocksWorldRender() const = 0;

    virtual void tick() const = 0;

    static void drawRect(float x, float y, float w, float h, const glm::vec4 & color);
};