#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <glad/glad.hpp>
#include "Shader.hpp"

class FontRenderer {
public:
    FontRenderer(const std::string& texturePath, Shader* shader, int charWidth = 8, int charHeight = 8);
    ~FontRenderer();

    Shader* shader;
    GLuint textureID{};

    void renderText(const std::string &text, float x, float y, float scale = 1.0f, unsigned int color = 0xFFFFFFFF, bool shadow = false) const;
    void renderTextWithDepth(const std::string& text, float x, float y, float scale,
                             unsigned int color, bool depthTest) const;
    void renderTextStyled(const std::string& text, float x, float y, float scale,
                          unsigned int color, bool shadow, bool italic) const;
    void renderTextRotated(const std::string& text, float x, float y, float scale,
                           unsigned int color, bool shadow, float originX, float originY,
                           float angleRadians) const;
    [[nodiscard]] int getStringWidth(const std::string& text) const;

private:
    GLuint VAO{}, VBO{};
    int charWidth, charHeight;

    std::array<int, 256> glyphWidth{};
    std::array<uint8_t, 65536> unicodeGlyphWidth{};
    mutable std::array<GLuint, 256> unicodeTextures{};
    int atlasWidth{};
    int atlasHeight{};
    int atlasCellWidth{};
    int atlasCellHeight{};
    int atlasCellStrideX{};
    int atlasCellStrideY{};
    mutable std::vector<float> vertexScratch;
    mutable GLsizeiptr vboCapacity = 0;

    void loadTexture(const std::string& path);
    void setupMesh();
    [[nodiscard]] int codepointWidth(uint32_t codepoint) const;
    [[nodiscard]] GLuint unicodeTexture(uint8_t page) const;
    void renderTextInternal(const std::string& text, float x, float y, float scale,
                            unsigned int color, bool shadow, float originX, float originY,
                            float angleRadians, bool italic = false,
                            bool depthTest = false) const;
    void renderFormatted(const std::string& text, float x, float y, float scale,
                         unsigned int color, bool shadow, bool italic,
                         bool depthTest) const;

};
