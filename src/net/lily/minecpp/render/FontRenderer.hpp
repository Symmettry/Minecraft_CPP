#pragma once
#include <string>
#include <glad/glad.h>
#include "Shader.hpp"

class FontRenderer {
public:
    FontRenderer(const std::string& texturePath, Shader* shader, int charWidth = 8, int charHeight = 8);
    ~FontRenderer();

    void renderText(const std::string& text, float x, float y, float scale = 1.0f, bool shadow = false) const;

    Shader* shader;
    GLuint textureID;

private:
    GLuint VAO, VBO;
    int charWidth, charHeight;

    void loadTexture(const std::string& path);
    void setupMesh();
};
