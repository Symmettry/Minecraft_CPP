#pragma once

#include <array>
#include <cmath>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "lib/stb_image.hpp"
#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/menu/Gui.hpp"
#include "net/lily/minecpp/menu/impl/GuiMultiplayer.hpp"
#include "net/lily/minecpp/menu/impl/GuiOptions.hpp"

class GuiMainMenu final : public Gui {
    enum class Action { Singleplayer, Multiplayer, Realms, Options, Quit };

    struct Button {
        const char* label;
        Action action;
        bool enabled;
    };

    static constexpr std::array buttons{
        Button{"Singleplayer", Action::Singleplayer, true},
        Button{"Multiplayer", Action::Multiplayer, true},
        Button{"Minecraft Realms", Action::Realms, true},
        Button{"Options...", Action::Options, true},
        Button{"Quit Game", Action::Quit, true}
    };

    mutable double mouseX = -1.0;
    mutable double mouseY = -1.0;
    mutable std::string status;
    mutable std::string splashText = "missingno";

    static inline GLuint panoramaTexture = 0;
    static inline GLuint panoramaVao = 0;
    static inline GLuint panoramaVbo = 0;
    static inline GLuint titleTexture = 0;
    static inline GLuint widgetsTexture = 0;
    static inline std::array<GLuint, 2> panoramaFramebuffers{};
    static inline std::array<GLuint, 2> panoramaFrames{};
    static inline Shader* panoramaShader = nullptr;
    static inline Shader* panoramaBlurShader = nullptr;

    struct Bounds {
        float x;
        float y;
        float width;
        float height;
    };

    [[nodiscard]] int scaleFactor() const {
        int factor = 1;
        const int requested = mc->settings->guiScale == 0 ? 1000 : mc->settings->guiScale;
        while (factor < requested && mc->width() / (factor + 1) >= 320
               && mc->height() / (factor + 1) >= 240) {
            ++factor;
        }
        return factor;
    }

    [[nodiscard]] int guiWidth() const {
        const int factor = scaleFactor();
        return (mc->width() + factor - 1) / factor;
    }

    [[nodiscard]] int guiHeight() const {
        const int factor = scaleFactor();
        return (mc->height() + factor - 1) / factor;
    }

    [[nodiscard]] Bounds buttonBounds(const size_t index) const {
        const float center = static_cast<float>(guiWidth()) * 0.5f;
        const float firstY = static_cast<float>(guiHeight() / 4 + 48);
        if (index < 3) return {center - 100.0f, firstY + static_cast<float>(index) * 24.0f, 200.0f, 20.0f};
        return index == 3
            ? Bounds{center - 100.0f, firstY + 84.0f, 98.0f, 20.0f}
            : Bounds{center + 2.0f, firstY + 84.0f, 98.0f, 20.0f};
    }

    static void initTextures() {
        if (panoramaTexture != 0) return;

        titleTexture = loadTexture("assets/minecraft/textures/gui/title/minecraft.png");
        widgetsTexture = loadTexture("assets/minecraft/textures/gui/widgets.png");

        glGenTextures(1, &panoramaTexture);
        glBindTexture(GL_TEXTURE_CUBE_MAP, panoramaTexture);
        constexpr std::array faceFiles{1, 3, 4, 5, 0, 2};
        for (size_t face = 0; face < faceFiles.size(); ++face) {
            const std::string path = "assets/minecraft/textures/gui/title/background/panorama_"
                + std::to_string(faceFiles[face]) + ".png";
            int width = 0;
            int height = 0;
            int channels = 0;
            stbi_set_flip_vertically_on_load(false);
            unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 3);
            if (pixels == nullptr) throw std::runtime_error("Unable to load panorama texture: " + path);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(face), 0,
                GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
            stbi_image_free(pixels);
        }
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        panoramaShader = new Shader("assets/shaders/panorama.vert", "assets/shaders/panorama.frag");
        panoramaBlurShader = new Shader("assets/shaders/panorama_blur.vert", "assets/shaders/panorama_blur.frag");
        constexpr float vertices[] = {
            -1, 1,-1, -1,-1,-1,  1,-1,-1,  1,-1,-1,  1, 1,-1, -1, 1,-1,
            -1,-1, 1, -1,-1,-1, -1, 1,-1, -1, 1,-1, -1, 1, 1, -1,-1, 1,
             1,-1,-1,  1,-1, 1,  1, 1, 1,  1, 1, 1,  1, 1,-1,  1,-1,-1,
            -1,-1, 1, -1, 1, 1,  1, 1, 1,  1, 1, 1,  1,-1, 1, -1,-1, 1,
            -1, 1,-1,  1, 1,-1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1, 1,-1,
            -1,-1,-1, -1,-1, 1,  1,-1,-1,  1,-1,-1, -1,-1, 1,  1,-1, 1
        };
        glGenVertexArrays(1, &panoramaVao);
        glGenBuffers(1, &panoramaVbo);
        glBindVertexArray(panoramaVao);
        glBindBuffer(GL_ARRAY_BUFFER, panoramaVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        glBindVertexArray(0);

        glGenFramebuffers(2, panoramaFramebuffers.data());
        glGenTextures(2, panoramaFrames.data());
        for (size_t i = 0; i < panoramaFrames.size(); ++i) {
            glBindTexture(GL_TEXTURE_2D, panoramaFrames[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindFramebuffer(GL_FRAMEBUFFER, panoramaFramebuffers[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, panoramaFrames[i], 0);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                throw std::runtime_error("Unable to create panorama framebuffer");
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    static GLuint renderPanorama() {
        glBindFramebuffer(GL_FRAMEBUFFER, panoramaFramebuffers[0]);
        glViewport(0, 0, 256, 256);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        panoramaShader->use();
        const glm::mat4 projection = glm::perspective(glm::radians(120.0f), 1.0f, 0.05f, 10.0f);
        glm::mat4 view{1.0f};
        const float timer = static_cast<float>(glfwGetTime()) * 20.0f;
        view = glm::rotate(view, glm::radians(std::sin(timer / 400.0f) * 25.0f + 20.0f), glm::vec3{1.0f, 0.0f, 0.0f});
        view = glm::rotate(view, glm::radians(-timer * 0.1f), glm::vec3{0.0f, 1.0f, 0.0f});
        panoramaShader->setMat4("projection", glm::value_ptr(projection));
        panoramaShader->setMat4("view", glm::value_ptr(view));
        panoramaShader->setInt("panorama", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, panoramaTexture);
        glBindVertexArray(panoramaVao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        panoramaShader->stop();

        int current = 0;
        panoramaBlurShader->use();
        panoramaBlurShader->setInt("sourceTexture", 0);
        for (int pass = 0; pass < 7; ++pass) {
            const int target = 1 - current;
            glBindFramebuffer(GL_FRAMEBUFFER, panoramaFramebuffers[target]);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, panoramaFrames[current]);
            glBindVertexArray(quadVAO);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            current = target;
        }
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        panoramaBlurShader->stop();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, mc->width(), mc->height());
        return panoramaFrames[current];
    }

    static void renderCenteredText(const FontRenderer* font, const std::string& text,
                                   const float centerX, const float y, const float scale,
                                   const unsigned int color) {
        const float width = static_cast<float>(font->getStringWidth(text)) * scale;
        font->renderText(text, centerX - width * 0.5f, y, scale, color, true);
    }

    void chooseSplashText() const {
        std::ifstream stream("assets/minecraft/texts/splashes.txt");
        std::vector<std::string> splashes;
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) splashes.push_back(line);
        }
        if (splashes.empty()) return;
        std::mt19937 generator{std::random_device{}()};
        std::uniform_int_distribution<size_t> distribution(0, splashes.size() - 1);
        splashText = splashes[distribution(generator)];
    }

    [[nodiscard]] bool isHovered(const size_t index) const {
        const Bounds bounds = buttonBounds(index);
        return mouseX >= bounds.x && mouseX < bounds.x + bounds.width
            && mouseY >= bounds.y && mouseY < bounds.y + bounds.height;
    }

    void activate(const size_t index) const {
        if (index >= buttons.size() || !buttons[index].enabled) return;

        switch (buttons[index].action) {
            case Action::Multiplayer:
                mc->displayGuiScreen(new GuiMultiplayer());
                break;
            case Action::Quit:
                mc->running = false;
                glfwSetWindowShouldClose(mc->renderer->window, GLFW_TRUE);
                break;
            case Action::Options:
                mc->displayGuiScreen(new GuiOptions());
                break;
            case Action::Singleplayer:
            case Action::Realms:
                status = "This screen is not implemented yet";
                break;
        }
    }

public:
    void onOpen() override {
        initTextures();
        chooseSplashText();
        glfwSetInputMode(mc->renderer->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    void onClose() override {
        glfwSetInputMode(mc->renderer->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    void update(float) override {}

    void render() const override {
        Gui::render();

        const int factor = scaleFactor();
        const float width = static_cast<float>(guiWidth());
        const float height = static_cast<float>(guiHeight());
        const float physicalWidth = static_cast<float>(mc->width());
        const float physicalHeight = static_cast<float>(mc->height());
        const GLuint panorama = renderPanorama();
        glDisable(GL_DEPTH_TEST);
        const float aspect = physicalWidth / physicalHeight;
        const float sourceWidth = aspect >= 1.0f ? 256.0f : 256.0f * aspect;
        const float sourceHeight = aspect >= 1.0f ? 256.0f / aspect : 256.0f;
        drawTexture(panorama, 0.0f, 0.0f, physicalWidth, physicalHeight,
            (256.0f - sourceWidth) * 0.5f, (256.0f - sourceHeight) * 0.5f,
            256.0f, 256.0f, sourceWidth, sourceHeight,
            {1.0f, 1.0f, 1.0f, 1.0f}, false);
        drawRect(0.0f, 0.0f, physicalWidth, physicalHeight, {0.0f, 0.0f, 0.0f, 0.30f});

        const float scaled = static_cast<float>(factor);
        // the two title-atlas halves contain asymmetric transparent padding vanilla uses width / 2 - 137 rather than geometrically centering 310 pixels
        const float logoX = (width * 0.5f - 137.0f) * scaled;
        drawTexture(titleTexture, logoX, 30.0f * scaled,
            155.0f * scaled, 44.0f * scaled, 0.0f, 0.0f, 256.0f, 256.0f, 155.0f, 44.0f);
        drawTexture(titleTexture, logoX + 155.0f * scaled, 30.0f * scaled,
            155.0f * scaled, 44.0f * scaled, 0.0f, 45.0f, 256.0f, 256.0f, 155.0f, 44.0f);

        const FontRenderer* font = mc->fontRenderer;
        font->shader->use();
        font->shader->setMat4("projection", glm::value_ptr(glm::ortho(0.0f, physicalWidth, physicalHeight, 0.0f)));

        const float pulse = 1.8f - std::abs(std::sin(static_cast<float>(glfwGetTime())
            * glm::two_pi<float>())) * 0.1f;
        const float splashScale = pulse * 100.0f
            / static_cast<float>(font->getStringWidth(splashText) + 32) * scaled;
        const float splashCenterX = (width * 0.5f + 90.0f) * scaled;
        const float splashCenterY = 70.0f * scaled;
        const float splashX = splashCenterX
            - static_cast<float>(font->getStringWidth(splashText)) * splashScale * 0.5f;
        font->renderTextRotated(splashText, splashX, splashCenterY - 8.0f * splashScale,
            splashScale, 0xFFFFFF00, true, splashCenterX, splashCenterY, glm::radians(-20.0f));

        for (size_t i = 0; i < buttons.size(); ++i) {
            const Bounds bounds = buttonBounds(i);
            const bool highlighted = buttons[i].enabled && isHovered(i);
            glDisable(GL_DEPTH_TEST);
            const float textureY = !buttons[i].enabled ? 46.0f : highlighted ? 86.0f : 66.0f;
            const float half = bounds.width * 0.5f;
            drawTexture(widgetsTexture, bounds.x * scaled, bounds.y * scaled,
                half * scaled, bounds.height * scaled, 0.0f, textureY,
                256.0f, 256.0f, half, bounds.height);
            drawTexture(widgetsTexture, (bounds.x + half) * scaled, bounds.y * scaled,
                half * scaled, bounds.height * scaled, 200.0f - half, textureY,
                256.0f, 256.0f, half, bounds.height);

            renderCenteredText(font, buttons[i].label,
                (bounds.x + bounds.width * 0.5f) * scaled,
                (bounds.y + (bounds.height - 8.0f) * 0.5f) * scaled,
                scaled, !buttons[i].enabled ? 0xFFA0A0A0 : highlighted ? 0xFFFFFFA0 : 0xFFE0E0E0);
        }

        if (!status.empty()) {
            renderCenteredText(font, status, width * 0.5f * scaled,
                (static_cast<float>(guiHeight() / 4 + 154)) * scaled, scaled, 0xFFFFC85C);
        }

        font->renderText("Minecraft 1.8.9", 2.0f * scaled, (height - 10.0f) * scaled,
            scaled, 0xFFFFFFFF, true);
        const std::string copyright = "Copyright Mojang AB. Do not distribute!";
        font->renderText(copyright,
            (width - static_cast<float>(font->getStringWidth(copyright)) - 2.0f) * scaled,
            (height - 10.0f) * scaled, scaled, 0xFFFFFFFF, true);
    }

    bool handleKey(int, int, int, int) override { return false; }

    bool handleMouseButton(const int button, const int action, int) override {
        if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return false;
        for (size_t i = 0; i < buttons.size(); ++i) {
            if (isHovered(i)) {
                activate(i);
                return true;
            }
        }
        return false;
    }

    bool handleCursorPos(const double x, const double y) override {
        int windowWidth = 1;
        int windowHeight = 1;
        glfwGetWindowSize(mc->renderer->window, &windowWidth, &windowHeight);
        const double framebufferX = x * static_cast<double>(mc->width()) / static_cast<double>(windowWidth);
        const double framebufferY = y * static_cast<double>(mc->height()) / static_cast<double>(windowHeight);
        mouseX = framebufferX / static_cast<double>(scaleFactor());
        mouseY = framebufferY / static_cast<double>(scaleFactor());
        return true;
    }

    [[nodiscard]] bool blocksWorldUpdate() const override { return true; }
    [[nodiscard]] bool blocksWorldRender() const override { return true; }
    void tick() const override {}
};
