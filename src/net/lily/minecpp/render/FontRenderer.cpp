#define FLOAT static_cast<float>

#include "FontRenderer.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

#include "lib/stb_image.hpp"
#include <iostream>
#include <glm/vec4.hpp>
#include <tracy/Tracy.hpp>

#include "net/lily/minecpp/util/Math.hpp"
#include "net/lily/minecpp/util/Utf8.hpp"

namespace {
int defaultGlyphIndex(const uint32_t codepoint) {
    if (codepoint < 128) return static_cast<int>(codepoint);
    static constexpr char32_t leading[]{
        U'À', U'Á', U'Â', U'È', U'Ê', U'Ë', U'Í', U'Ó', U'Ô', U'Õ', U'Ú', U'ß', U'ã', U'õ', U'ğ', U'İ',
        U'ı', U'Œ', U'œ', U'Ş', U'ş', U'Ŵ', U'ŵ', U'ž', U'ȇ'};
    if (const auto found = std::find(std::begin(leading), std::end(leading), static_cast<char32_t>(codepoint));
        found != std::end(leading)) return static_cast<int>(found - std::begin(leading));
    static constexpr char32_t extended[]{
        U'Ç', U'ü', U'é', U'â', U'ä', U'à', U'å', U'ç', U'ê', U'ë', U'è', U'ï', U'î', U'ì', U'Ä', U'Å',
        U'É', U'æ', U'Æ', U'ô', U'ö', U'ò', U'û', U'ù', U'ÿ', U'Ö', U'Ü', U'ø', U'£', U'Ø', U'×', U'ƒ',
        U'á', U'í', U'ó', U'ú', U'ñ', U'Ñ', U'ª', U'º', U'¿', U'®', U'¬', U'½', U'¼', U'¡', U'«', U'»',
        U'░', U'▒', U'▓', U'│', U'┤', U'╡', U'╢', U'╖', U'╕', U'╣', U'║', U'╗', U'╝', U'╜', U'╛', U'┐',
        U'└', U'┴', U'┬', U'├', U'─', U'┼', U'╞', U'╟', U'╚', U'╔', U'╩', U'╦', U'╠', U'═', U'╬', U'╧',
        U'╨', U'╤', U'╥', U'╙', U'╘', U'╒', U'╓', U'╫', U'╪', U'┘', U'┌', U'█', U'▄', U'▌', U'▐', U'▀',
        U'α', U'β', U'Γ', U'π', U'Σ', U'σ', U'μ', U'τ', U'Φ', U'Θ', U'Ω', U'δ', U'∞', U'∅', U'∈', U'∩',
        U'≡', U'±', U'≥', U'≤', U'⌠', U'⌡', U'÷', U'≈', U'°', U'∙', U'·', U'√', U'ⁿ', U'²', U'■', U'\0'};
    const auto found = std::find(std::begin(extended), std::end(extended), static_cast<char32_t>(codepoint));
    return found == std::end(extended) ? -1 : 128 + static_cast<int>(found - std::begin(extended));
}
}

FontRenderer::FontRenderer(const std::string& texturePath, Shader* shader, const int charWidth, const int charHeight)
    : shader(shader), charWidth(charWidth), charHeight(charHeight) {
    loadTexture(texturePath);
    std::ifstream glyphs("assets/minecraft/font/glyph_sizes.bin", std::ios::binary);
    if (glyphs) glyphs.read(reinterpret_cast<char*>(unicodeGlyphWidth.data()), unicodeGlyphWidth.size());
    setupMesh();
}

FontRenderer::~FontRenderer() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteTextures(1, &textureID);
    for (const GLuint texture : unicodeTextures) if (texture != 0) glDeleteTextures(1, &texture);
}

void FontRenderer::loadTexture(const std::string& path) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(false);

    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        exit(EXIT_FAILURE);
    }

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

    // gives every atlas cell a transparent gutter. nearest-neighbor sampling at a quad edge can otherwise select a pixel from the adjacent character, which then makes weird lines
    atlasCellWidth = cellW;
    atlasCellHeight = cellH;
    atlasCellStrideX = cellW + 2;
    atlasCellStrideY = cellH + 2;
    atlasWidth = atlasCellStrideX * 16;
    atlasHeight = atlasCellStrideY * 16;
    std::vector<unsigned char> padded(static_cast<size_t>(atlasWidth * atlasHeight * 4), 0);
    for (int cellY = 0; cellY < 16; ++cellY) {
        for (int cellX = 0; cellX < 16; ++cellX) {
            for (int y = 0; y < cellH; ++y) {
                for (int x = 0; x < cellW; ++x) {
                    const size_t source = static_cast<size_t>(((cellY * cellH + y) * width + cellX * cellW + x) * 4);
                    const size_t target = static_cast<size_t>(((cellY * atlasCellStrideY + y + 1) * atlasWidth
                        + cellX * atlasCellStrideX + x + 1) * 4);
                    std::copy_n(data + source, 4, padded.data() + target);
                }
            }
        }
    }

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlasWidth, atlasHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, padded.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    stbi_image_free(data);
}


void FontRenderer::setupMesh() {
    // only 6 vertices (2 triangles) for a single quad
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
    // allocate and upload initial vertex data
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));

    glBindVertexArray(0);
}

int FontRenderer::getStringWidth(const std::string& text) const {
    int width = 0;
    for (size_t offset = 0; offset < text.size();) {
        const uint32_t character = Utf8::next(text, offset);
        if (character == 0xa7 && offset < text.size()) {
            Utf8::next(text, offset);
            continue;
        }
        width += codepointWidth(character);
    }
    return width;
}

int FontRenderer::codepointWidth(const uint32_t codepoint) const {
    if (codepoint == ' ') return 4;
    if (const int glyph = defaultGlyphIndex(codepoint); glyph >= 0) return glyphWidth[glyph];
    if (codepoint > 0xffff) return glyphWidth['?'];
    const uint8_t bounds = unicodeGlyphWidth[codepoint];
    if (bounds == 0) return 0;
    int left = bounds >> 4U;
    int right = bounds & 15U;
    if (right > 7) { left = 0; right = 15; }
    return (right + 1 - left) / 2 + 1;
}

GLuint FontRenderer::unicodeTexture(const uint8_t page) const {
    if (unicodeTextures[page] != 0) return unicodeTextures[page];
    char path[80];
    std::snprintf(path, sizeof(path), "assets/minecraft/textures/font/unicode_page_%02x.png", page);
    int width = 0, height = 0, channels = 0;
    unsigned char* const pixels = stbi_load(path, &width, &height, &channels, 4);
    if (pixels == nullptr) return textureID;
    glGenTextures(1, &unicodeTextures[page]);
    glBindTexture(GL_TEXTURE_2D, unicodeTextures[page]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(pixels);
    return unicodeTextures[page];
}

void FontRenderer::renderText(const std::string& text, const float x, const float y, const float scale, const unsigned int color, const bool shadow) const {
    renderFormatted(text, x, y, scale, color, shadow, false, false);
}

void FontRenderer::renderTextWithDepth(const std::string& text, const float x, const float y,
                                       const float scale, const unsigned int color,
                                       const bool depthTest) const {
    renderFormatted(text, x, y, scale, color, false, false, depthTest);
}

void FontRenderer::renderTextStyled(const std::string& text, const float x, const float y,
                                    const float scale, const unsigned int color,
                                    const bool shadow, const bool italic) const {
    renderFormatted(text, x, y, scale, color, shadow, italic, false);
}

void FontRenderer::renderFormatted(const std::string& text, float x, const float y,
                                   const float scale, const unsigned int color,
                                   const bool shadow, const bool baseItalic,
                                   const bool depthTest) const {
    static constexpr unsigned int colors[16]{
        0xFF000000, 0xFF0000AA, 0xFF00AA00, 0xFF00AAAA,
        0xFFAA0000, 0xFFAA00AA, 0xFFFFAA00, 0xFFAAAAAA,
        0xFF555555, 0xFF5555FF, 0xFF55FF55, 0xFF55FFFF,
        0xFFFF5555, 0xFFFF55FF, 0xFFFFFF55, 0xFFFFFFFF};
    unsigned int currentColor = color;
    bool italic = baseItalic;
    std::string segment;
    const auto flush = [&] {
        if (segment.empty()) return;
        renderTextInternal(segment, x, y, scale, currentColor, shadow,
            0.0f, 0.0f, 0.0f, italic, depthTest);
        x += static_cast<float>(getStringWidth(segment)) * scale;
        segment.clear();
    };
    for (size_t index = 0; index < text.size(); ++index) {
        unsigned char character = text[index];
        size_t codeIndex = index;
        if (character == 0xC2 && index + 2 < text.size()
            && static_cast<unsigned char>(text[index + 1]) == 0xA7) codeIndex = index + 2;
        else if (character == 0xA7 && index + 1 < text.size()) codeIndex = index + 1;
        else {
            segment.push_back(static_cast<char>(character));
            continue;
        }
        flush();
        const char code = static_cast<char>(std::tolower(
            static_cast<unsigned char>(text[codeIndex])));
        if (code >= '0' && code <= '9')
            currentColor = (color & 0xFF000000U) | (colors[code - '0'] & 0x00FFFFFFU);
        else if (code >= 'a' && code <= 'f')
            currentColor = (color & 0xFF000000U) | (colors[10 + code - 'a'] & 0x00FFFFFFU);
        else if (code == 'o') italic = true;
        else if (code == 'r') { currentColor = color; italic = baseItalic; }
        index = codeIndex;
    }
    flush();
}

void FontRenderer::renderTextRotated(const std::string& text, const float x, const float y, const float scale,
                                     const unsigned int color, const bool shadow, const float originX,
                                     const float originY, const float angleRadians) const {
    renderTextInternal(text, x, y, scale, color, shadow, originX, originY, angleRadians);
}

void FontRenderer::renderTextInternal(const std::string& text, const float x, const float y, const float scale,
                                      const unsigned int color, const bool shadow, const float originX,
                                      const float originY, const float angleRadians, const bool italic,
                                      const bool depthTest) const {
    ZoneScopedN("Text batch");
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (depthTest) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);

    shader->use();
    shader->setInt("fontTexture", 0);        // match uniform
    shader->setVec2("rotationOrigin", originX, originY);
    shader->setFloat("rotationRadians", angleRadians);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glBindVertexArray(VAO);

    const auto drawPass = [&](const float startX, const float startY, const glm::vec4& passColor) {
        vertexScratch.clear();
        vertexScratch.reserve(text.size() * 24);
        float cursorX = startX;
        GLuint activeTexture = 0;
        const auto flush = [&] {
            if (vertexScratch.empty()) return;
            glBindTexture(GL_TEXTURE_2D, activeTexture);
            shader->setVec4("textColor", passColor);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            const GLsizeiptr uploadSize = static_cast<GLsizeiptr>(vertexScratch.size() * sizeof(float));
            if (uploadSize > vboCapacity) {
                glBufferData(GL_ARRAY_BUFFER, uploadSize, vertexScratch.data(), GL_STREAM_DRAW);
                vboCapacity = uploadSize;
            } else {
                glBufferSubData(GL_ARRAY_BUFFER, 0, uploadSize, vertexScratch.data());
            }
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertexScratch.size() / 4));
            vertexScratch.clear();
        };

        for (size_t offset = 0; offset < text.size();) {
            uint32_t codepoint = Utf8::next(text, offset);
            if (codepoint > 0xffff) codepoint = '?';
            const int advance = codepointWidth(codepoint);
            if (advance <= 0) continue;

            const int defaultGlyph = defaultGlyphIndex(codepoint);
            const bool unicode = defaultGlyph < 0;
            const GLuint glyphTexture = unicode ? unicodeTexture(static_cast<uint8_t>(codepoint >> 8U)) : textureID;
            if (activeTexture != glyphTexture) {
                flush();
                activeTexture = glyphTexture;
            }
            float tx0, ty0, tx1, ty1, w;
            if (unicode) {
                const uint8_t bounds = unicodeGlyphWidth[codepoint];
                int left = bounds >> 4U;
                int right = bounds & 15U;
                if (right > 7) { left = 0; right = 15; }
                tx0 = (FLOAT((codepoint & 15U) * 16U + left)) / 256.0f;
                ty0 = (FLOAT((codepoint & 255U) / 16U * 16U)) / 256.0f;
                tx1 = (FLOAT((codepoint & 15U) * 16U + right + 1U) - 0.02f) / 256.0f;
                ty1 = (FLOAT((codepoint & 255U) / 16U * 16U) + 15.98f) / 256.0f;
                w = FLOAT(right + 1 - left) * 0.5f * scale;
            } else {
                const int gw = glyphWidth[defaultGlyph];
                tx0 = FLOAT((defaultGlyph % 16) * atlasCellStrideX + 1) / FLOAT(atlasWidth);
                ty0 = FLOAT((defaultGlyph / 16) * atlasCellStrideY + 1) / FLOAT(atlasHeight);
                tx1 = tx0 + FLOAT(gw) / FLOAT(atlasWidth) - 0.01f / FLOAT(atlasWidth);
                ty1 = ty0 + FLOAT(atlasCellHeight) / FLOAT(atlasHeight) - 0.01f / FLOAT(atlasHeight);
                w = FLOAT(gw) * scale;
            }
            const float h = FLOAT(charHeight) * scale;
            const float shear = italic ? scale * 2.0f : 0.0f;
            const float glyph[] = {
                cursorX,     startY + h, tx0, ty1,
                cursorX + w, startY + h, tx1, ty1,
                cursorX + w + shear, startY, tx1, ty0,
                cursorX + shear, startY, tx0, ty0,
                cursorX,     startY + h, tx0, ty1,
                cursorX + w + shear, startY, tx1, ty0
            };
            vertexScratch.insert(vertexScratch.end(), std::begin(glyph), std::end(glyph));
            cursorX += FLOAT(advance) * scale;
        }
        flush();
    };

    if (shadow) {
        const float shadowOffset = 1.0f * scale;
        drawPass(x + shadowOffset, y + shadowOffset, Math::mcColorToVec4(color, true));
    }

    drawPass(x, y, Math::mcColorToVec4(color, false));

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}
