#include "Gui.hpp"
#include "../Minecraft.hpp"

GLuint Gui::quadVAO = 0;
GLuint Gui::quadVBO = 0;
GLuint Gui::shaderProgram = 0;
const Minecraft *Gui::mc;
Shader* Gui::quadShader;

void Gui::init() {
    mc = Minecraft::getMinecraft();
    // initQuad();
}

void Gui::initQuad() {
    if (quadVAO != 0) return;

    constexpr float vertices[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    quadShader = new Shader("assets/shaders/quad.vert", "assets/shaders/quad.frag");
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

    glUniform2f(glGetUniformLocation(quadShader->ID, "uPos"), ndcX, ndcY - ndcH);
    glUniform2f(glGetUniformLocation(quadShader->ID, "uSize"), ndcW, ndcH);
    glUniform4fv(glGetUniformLocation(quadShader->ID, "uColor"), 1, &color[0]);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);

    quadShader->stop();
}