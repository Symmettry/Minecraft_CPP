#pragma once
#define FLOAT static_cast<float>
#include <string>

#include "glad/glad.hpp"
#include "net/lily/minecpp/render/Shader.hpp"

struct Minecraft;

class Gui {
protected:
    static GLuint quadVAO;
    static GLuint quadVBO;
    static GLuint shaderProgram;
    static Shader* quadShader;
    static GLint quadPosLocation;
    static GLint quadSizeLocation;
    static GLint quadColorLocation;
    static GLint quadUvLocation;
    static GLint quadTextureLocation;
    static GLint quadUseTextureLocation;
    static GLint quadPixelSnapLocation;

    static void initQuad();
public:

    static void init();
    static Minecraft* mc;

    virtual ~Gui() = default;

    virtual void onOpen() {

    }
    virtual void onClose() {

    }

    virtual void update(float deltaSeconds) = 0;
    virtual void render() const;

    virtual bool handleKey(int key, int scancode, int action, int mods) = 0;
    virtual bool handleChar(unsigned int codepoint) { return false; }
    virtual bool handleMouseButton(int button, int action, int mods) = 0;
    virtual bool handleCursorPos(double x, double y) = 0;
    virtual bool handleScroll(double xOffset, double yOffset) { return false; }

    [[nodiscard]] virtual bool blocksWorldUpdate() const = 0;
    [[nodiscard]] virtual bool blocksWorldRender() const = 0;
    [[nodiscard]] virtual bool pausesGame() const { return false; }
    [[nodiscard]] virtual bool capturesMouse() const { return false; }

    virtual void tick() const = 0;

    static GLuint loadTexture(const std::string& path);
    static void drawRect(float x, float y, float w, float h, const glm::vec4 & color);
    static void drawTexture(GLuint texture, float x, float y, float w, float h,
                            float u, float v, float textureWidth, float textureHeight,
                            float regionWidth, float regionHeight,
                            const glm::vec4& color = {1.0f, 1.0f, 1.0f, 1.0f},
                            bool pixelSnap = true);
};
