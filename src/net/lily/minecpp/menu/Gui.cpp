#include "Gui.hpp"
#include "../Minecraft.hpp"
#include "lib/stb_image.hpp"

GLuint Gui::quadVAO = 0;
GLuint Gui::quadVBO = 0;
GLuint Gui::shaderProgram = 0;
Minecraft* Gui::mc;
Shader* Gui::quadShader;
GLint Gui::quadPosLocation = -1;
GLint Gui::quadSizeLocation = -1;
GLint Gui::quadColorLocation = -1;
GLint Gui::quadUvLocation = -1;
GLint Gui::quadTextureLocation = -1;
GLint Gui::quadUseTextureLocation = -1;
GLint Gui::quadPixelSnapLocation = -1;

void Gui::init() {
    mc = Minecraft::getMinecraft();
    initQuad();
}

void Gui::initQuad() {
    if (quadVAO != 0) return;

    constexpr float vertices[] = {
        0.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    quadShader = new Shader("assets/shaders/quad.vert", "assets/shaders/quad.frag");
    quadPosLocation = glGetUniformLocation(quadShader->ID, "uPos");
    quadSizeLocation = glGetUniformLocation(quadShader->ID, "uSize");
    quadColorLocation = glGetUniformLocation(quadShader->ID, "uColor");
    quadUvLocation = glGetUniformLocation(quadShader->ID, "uUvRect");
    quadTextureLocation = glGetUniformLocation(quadShader->ID, "uTexture");
    quadUseTextureLocation = glGetUniformLocation(quadShader->ID, "uUseTexture");
    quadPixelSnapLocation = glGetUniformLocation(quadShader->ID, "uPixelSnap");
}

void Gui::render() const {
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Gui::drawRect(const float x, const float y, const float w, const float h, const glm::vec4 &color) {
    quadShader->use();

    const float ndcX = (x / mc->width()) * 2.0f - 1.0f;
    const float ndcY = 1.0f - (y / mc->height()) * 2.0f;
    const float ndcW = (w / mc->width()) * 2.0f;
    const float ndcH = (h / mc->height()) * 2.0f;

    glUniform2f(quadPosLocation, ndcX, ndcY - ndcH);
    glUniform2f(quadSizeLocation, ndcW, ndcH);
    glUniform4fv(quadColorLocation, 1, &color[0]);
    glUniform1i(quadUseTextureLocation, GL_FALSE);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

GLuint Gui::loadTexture(const std::string& path) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (pixels == nullptr) throw std::runtime_error("Unable to load GUI texture: " + path);

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(pixels);
    return texture;
}

void Gui::drawTexture(const GLuint texture, const float x, const float y, const float w, const float h,
                      const float u, const float v, const float textureWidth, const float textureHeight,
                      const float regionWidth, const float regionHeight, const glm::vec4& color,
                      const bool pixelSnap) {
    quadShader->use();

    const float ndcX = x / mc->width() * 2.0f - 1.0f;
    const float ndcY = 1.0f - y / mc->height() * 2.0f;
    const float ndcW = w / mc->width() * 2.0f;
    const float ndcH = h / mc->height() * 2.0f;

    glUniform2f(quadPosLocation, ndcX, ndcY - ndcH);
    glUniform2f(quadSizeLocation, ndcW, ndcH);
    glUniform4fv(quadColorLocation, 1, &color[0]);
    glUniform4f(quadUvLocation,
        u / textureWidth, v / textureHeight, regionWidth / textureWidth, regionHeight / textureHeight);
    glUniform1i(quadTextureLocation, 0);
    glUniform1i(quadUseTextureLocation, GL_TRUE);
    glUniform1i(quadPixelSnapLocation, pixelSnap ? GL_TRUE : GL_FALSE);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}
