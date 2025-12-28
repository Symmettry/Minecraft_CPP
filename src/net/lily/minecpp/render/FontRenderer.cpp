#define FLOAT static_cast<float>

#include "FontRenderer.hpp"
#include <filesystem>

#include "lib/stb_image.h"
#include <iostream>
#include <glm/vec4.hpp>

#include "net/lily/minecpp/util/Math.hpp"

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
    stbi_set_flip_vertically_on_load(false);

    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        exit(EXIT_FAILURE);
    }

    atlasWidth = width;
    atlasHeight = height;

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    const int cellW = width / 16;
    const int cellH = height / 16;
    const float scale = 8.0f / FLOAT(cellW);
    constexpr int padding = 1;

    for (int ch = 0; ch < 256; ++ch) {
        if (ch == ' ') {
            glyphWidth[ch] = 3 + padding;
            continue;
        }

        const int cx = ch % 16;
        const int cy = ch / 16;

        int x;
        for (x = cellW - 1; x >= 0; --x) {
            bool empty = true;

            for (int y = 0; y < cellH && empty; ++y) {
                const int px = cx * cellW + x;
                const int py = cy * cellH + y;
                const int idx = (py * width + px) * 4;
                if (data[idx + 3] != 0) {
                    empty = false;
                }
            }

            if (!empty) {
                break;
            }
        }

        ++x;
        glyphWidth[ch] = x + padding;
    }

    stbi_image_free(data);
}


void FontRenderer::setupMesh() {
    // Only 6 vertices (2 triangles) for a single quad
    constexpr float vertices[24] = {
        // positions   // uv
        0.0f, 0.0f,    0.0f, 0.0f,
        1.0f, 0.0f,    1.0f, 0.0f,
        1.0f, 1.0f,    1.0f, 1.0f,
        0.0f, 1.0f,    0.0f, 1.0f,
        0.0f, 0.0f,    0.0f, 0.0f,
        1.0f, 1.0f,    1.0f, 1.0f
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // Allocate and upload initial vertex data
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));

    glBindVertexArray(0);
}

void FontRenderer::renderText(const std::string& text, const float x, const float y, const float scale, const unsigned int color, const bool shadow) const {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    shader->use();
    shader->setInt("fontTexture", 0);        // match uniform
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glBindVertexArray(VAO);

    constexpr float texSize = 1.0f / 16.0f;

    auto drawChar = [&](const unsigned char c, const float xpos, const float ypos) {
        const int gw = glyphWidth[c];
        if (gw <= 0) {
            return;
        }

        const float w = FLOAT(gw) * scale;
        const float h = FLOAT(charHeight) * scale;

        const int cellWidth = atlasWidth / 16;

        const float tx = FLOAT(c % 16) * texSize;
        const float ty = FLOAT(c / 16) * texSize;
        const float tw = texSize * (FLOAT(gw) / FLOAT(cellWidth));

        const float vertices[24] = {
            xpos,     ypos + h, tx,          ty + texSize,
            xpos + w, ypos + h, tx + tw,     ty + texSize,
            xpos + w, ypos,     tx + tw,     ty,
            xpos,     ypos,     tx,          ty,
            xpos,     ypos + h, tx,          ty + texSize,
            xpos + w, ypos,     tx + tw,     ty
        };

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    };

    if (shadow) {
        const float shadowOffset = 1.0f * scale;
        float sx = x + shadowOffset;
        const float sy = y + shadowOffset;

        shader->setVec4("textColor", Math::mcColorToVec4(color, true));

        for (const unsigned char ch : text) {
            drawChar(ch, sx, sy);
            sx += glyphWidth[ch] * scale;
        }
    }

    shader->setVec4("textColor", Math::mcColorToVec4(color, false));

    float xpos = x;
    for (const unsigned char ch : text) {
        drawChar(ch, xpos, y);
        xpos += glyphWidth[ch] * scale;
    }

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}
