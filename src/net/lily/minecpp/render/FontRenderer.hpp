#pragma once
#include <string>
#include <glad/glad.h>
#include "Shader.hpp"

class FontRenderer {
public:
    FontRenderer(const std::string& texturePath, Shader* shader, int charWidth = 8, int charHeight = 8);
    ~FontRenderer();

    Shader* shader;
    GLuint textureID{};

    void renderText(const std::string &text, float x, float y, float scale = 1.0f, unsigned int color = 0xFFFFFFFF, bool shadow = false) const;

private:
    GLuint VAO{}, VBO{};
    int charWidth, charHeight;

    std::array<int, 256> glyphWidth{};
    int atlasWidth{};
    int atlasHeight{};

    void loadTexture(const std::string& path);
    void setupMesh();

};
