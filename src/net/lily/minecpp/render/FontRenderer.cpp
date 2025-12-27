#include "FontRenderer.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include <filesystem>

#include "lib/stb_image.h"
#include <iostream>

FontRenderer::FontRenderer(const std::string& texturePath, Shader* shader, const int charWidth, const int charHeight)
    : shader(shader), charWidth(charWidth), charHeight(charHeight) {
    loadTexture(texturePath);
    setupMesh();
}

FontRenderer::~FontRenderer() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteTextures(1, &textureID);
}

void FontRenderer::loadTexture(const std::string& path) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        exit(EXIT_FAILURE);
    }

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    stbi_image_free(data);
}

void FontRenderer::setupMesh() {
    float vertices[24] = {
        // pos      // uv
        0, 0,       0, 0,
        1, 0,       1, 0,
        1, 1,       1, 1,
        0, 1,       0, 1,
        0, 0,       0, 0,
        1, 1,       1, 1
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
}

void FontRenderer::renderText(const std::string& text, const float x, const float y, const float scale, const bool shadow) const {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    shader->use();
    shader->setInt("text", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glBindVertexArray(VAO);

    const auto drawChar = [&](const char c, const float xpos, const float ypos) {
        constexpr float texSize = 1.0f / 16.0f;
        const float tx = (c % 16) * texSize;
        const float ty = 1.0f - (c / 16) * texSize - texSize;

        const float vertices[24] = {
            xpos,                  ypos,                   tx,       ty + texSize,
            xpos + charWidth*scale, ypos,                  tx + texSize, ty + texSize,
            xpos + charWidth*scale, ypos + charHeight*scale, tx + texSize, ty,
            xpos,                  ypos + charHeight*scale, tx,       ty,
            xpos,                  ypos,                   tx,       ty + texSize,
            xpos + charWidth*scale, ypos + charHeight*scale, tx + texSize, ty
        };

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    };

    if (shadow) {
        for (size_t i = 0; i < text.size(); ++i)
            drawChar(text[i], x + 1.0f, y + 1.0f);
    }

    for (size_t i = 0; i < text.size(); ++i)
        drawChar(text[i], x + static_cast<float>(i) * charWidth * scale, y);

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}
