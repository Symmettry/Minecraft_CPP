#pragma once

#include <atomic>
#include <string>
#include <thread>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/menu/Gui.hpp"

class GuiDisconnected final : public Gui {
public:
    GuiDisconnected(Gui* parent, std::string title, std::string message)
        : parent(parent), title(std::move(title)), message(std::move(message)) {}

    void onOpen() override {
        if (backgroundTexture == 0) backgroundTexture = loadTexture("assets/minecraft/textures/gui/options_background.png");
        if (widgetsTexture == 0) widgetsTexture = loadTexture("assets/minecraft/textures/gui/widgets.png");
    }
    void onClose() override {}
    void update(float) override {}

    void render() const override {
        drawDirt();
        const float scale = static_cast<float>(scaleFactor());
        const float center = mc->width() * 0.5f;
        const FontRenderer* font = mc->fontRenderer;
        font->shader->use();
        font->shader->setMat4("projection", glm::value_ptr(glm::ortho(
            0.0f, static_cast<float>(mc->width()), static_cast<float>(mc->height()), 0.0f)));
        drawCentered(title, center, mc->height() * 0.5f - 50.0f * scale, 0xFFAAAAAA);
        drawCentered(message, center, mc->height() * 0.5f - 18.0f * scale, 0xFFFFFFFF);
        drawButton(doneBounds(), "Done");
    }

    bool handleKey(int key, int, int action, int) override {
        if (action != GLFW_PRESS) return false;
        if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
            mc->displayGuiScreen(parent);
            return true;
        }
        return false;
    }
    bool handleMouseButton(int button, int action, int) override {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && hovered(doneBounds())) {
            mc->displayGuiScreen(parent);
            return true;
        }
        return false;
    }
    bool handleCursorPos(double x, double y) override {
        updateMouse(x, y);
        return true;
    }
    [[nodiscard]] bool blocksWorldUpdate() const override { return true; }
    [[nodiscard]] bool blocksWorldRender() const override { return true; }
    void tick() const override {}

private:
    struct Bounds { float x, y, width, height; };
    Gui* parent;
    std::string title;
    std::string message;
    mutable double mouseX = -1.0;
    mutable double mouseY = -1.0;
    static inline GLuint backgroundTexture = 0;
    static inline GLuint widgetsTexture = 0;

    [[nodiscard]] int scaleFactor() const {
        int factor = 1;
        const int requested = mc->settings->guiScale == 0 ? 1000 : mc->settings->guiScale;
        while (factor < requested && mc->width() / (factor + 1) >= 320
            && mc->height() / (factor + 1) >= 240) ++factor;
        return factor;
    }
    [[nodiscard]] Bounds doneBounds() const {
        const float scale = static_cast<float>(scaleFactor());
        return {mc->width() * 0.5f - 100.0f * scale,
            mc->height() * 0.5f + 12.0f * scale, 200.0f * scale, 20.0f * scale};
    }
    [[nodiscard]] bool hovered(const Bounds& bounds) const {
        return mouseX >= bounds.x && mouseX < bounds.x + bounds.width
            && mouseY >= bounds.y && mouseY < bounds.y + bounds.height;
    }
    void updateMouse(const double x, const double y) const {
        int windowWidth = 1, windowHeight = 1;
        glfwGetWindowSize(mc->renderer->window, &windowWidth, &windowHeight);
        mouseX = x * mc->width() / windowWidth;
        mouseY = y * mc->height() / windowHeight;
    }
    void drawDirt() const {
        Gui::render();
        const float tile = 32.0f * scaleFactor();
        for (float y = 0; y < mc->height(); y += tile) {
            for (float x = 0; x < mc->width(); x += tile) {
                drawTexture(backgroundTexture, x, y, tile, tile, 0, 0, 32, 32, 32, 32,
                    {0.38f, 0.38f, 0.38f, 1.0f});
            }
        }
    }
    void drawCentered(const std::string& text, const float center, const float y,
                      const unsigned int color) const {
        const float scale = static_cast<float>(scaleFactor());
        mc->fontRenderer->renderText(text,
            center - mc->fontRenderer->getStringWidth(text) * scale * 0.5f, y, scale, color, true);
    }
    void drawButton(const Bounds& bounds, const char* label) const {
        const bool over = hovered(bounds);
        const float textureY = over ? 86.0f : 66.0f;
        const float half = bounds.width * 0.5f;
        drawTexture(widgetsTexture, bounds.x, bounds.y, half, bounds.height,
            0, textureY, 256, 256, 100, 20);
        drawTexture(widgetsTexture, bounds.x + half, bounds.y, half, bounds.height,
            100, textureY, 256, 256, 100, 20);
        drawCentered(label, bounds.x + bounds.width * 0.5f,
            bounds.y + (bounds.height - 8.0f * scaleFactor()) * 0.5f,
            over ? 0xFFFFFFA0 : 0xFFE0E0E0);
    }
};

class GuiDownloadTerrain final : public Gui {
public:
    void onOpen() override {
        if (backgroundTexture == 0) backgroundTexture = loadTexture("assets/minecraft/textures/gui/options_background.png");
    }
    void onClose() override {}
    void update(float) override {}
    void render() const override {
        Gui::render();
        const float scale = static_cast<float>(scaleFactor());
        const float tile = 32.0f * scale;
        for (float y = 0; y < mc->height(); y += tile) for (float x = 0; x < mc->width(); x += tile)
            drawTexture(backgroundTexture, x, y, tile, tile, 0, 0, 32, 32, 32, 32,
                {0.38f, 0.38f, 0.38f, 1.0f});
        const FontRenderer* font = mc->fontRenderer;
        font->shader->use();
        font->shader->setMat4("projection", glm::value_ptr(glm::ortho(
            0.0f, static_cast<float>(mc->width()), static_cast<float>(mc->height()), 0.0f)));
        constexpr const char* text = "Downloading terrain";
        font->renderText(text, mc->width() * 0.5f - font->getStringWidth(text) * scale * 0.5f,
            mc->height() * 0.5f - 50.0f * scale, scale, 0xFFFFFFFF, true);
    }
    bool handleKey(int, int, int, int) override { return true; }
    bool handleMouseButton(int, int, int) override { return true; }
    bool handleCursorPos(double, double) override { return true; }
    [[nodiscard]] bool blocksWorldUpdate() const override { return false; }
    [[nodiscard]] bool blocksWorldRender() const override { return true; }
    void tick() const override {}

private:
    static inline GLuint backgroundTexture = 0;
    [[nodiscard]] int scaleFactor() const {
        int factor = 1;
        const int requested = mc->settings->guiScale == 0 ? 1000 : mc->settings->guiScale;
        while (factor < requested && mc->width() / (factor + 1) >= 320
            && mc->height() / (factor + 1) >= 240) ++factor;
        return factor;
    }
};

class GuiConnecting final : public Gui {
public:
    GuiConnecting(Gui* parent, std::string host, const uint16_t port)
        : parent(parent), host(std::move(host)), port(port) {}

    void onOpen() override {
        if (backgroundTexture == 0) backgroundTexture = loadTexture("assets/minecraft/textures/gui/options_background.png");
        if (widgetsTexture == 0) widgetsTexture = loadTexture("assets/minecraft/textures/gui/widgets.png");
        if (started.exchange(true)) return;
        std::thread([this] {
            const bool connected = mc->connectToServer(host, port);
            if (cancelled.load(std::memory_order_acquire) && connected && mc->netClient != nullptr)
                mc->netClient->fail("Connection cancelled");
            result.store(connected ? Result::Connected : Result::Failed, std::memory_order_release);
        }).detach();
    }
    void onClose() override {}
    void update(float) override {}
    void render() const override {
        drawDirt();
        const float scale = static_cast<float>(scaleFactor());
        const FontRenderer* font = mc->fontRenderer;
        font->shader->use();
        font->shader->setMat4("projection", glm::value_ptr(glm::ortho(
            0.0f, static_cast<float>(mc->width()), static_cast<float>(mc->height()), 0.0f)));
        const std::string text = result.load(std::memory_order_acquire) == Result::Connected
            ? "Logging in..." : "Connecting to server...";
        font->renderText(text, mc->width() * 0.5f - font->getStringWidth(text) * scale * 0.5f,
            mc->height() * 0.5f - 50.0f * scale, scale, 0xFFFFFFFF, true);
        drawButton();
    }
    bool handleKey(int key, int, int action, int) override {
        if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
            cancelled.store(true, std::memory_order_release);
            if (mc->netClient != nullptr) mc->netClient->fail("Connection cancelled");
            mc->displayGuiScreen(parent);
            return true;
        }
        return false;
    }
    bool handleMouseButton(int button, int action, int) override {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && buttonHovered()) {
            cancelled.store(true, std::memory_order_release);
            if (mc->netClient != nullptr) mc->netClient->fail("Connection cancelled");
            mc->displayGuiScreen(parent);
            return true;
        }
        return false;
    }
    bool handleCursorPos(double x, double y) override {
        int windowWidth = 1, windowHeight = 1;
        glfwGetWindowSize(mc->renderer->window, &windowWidth, &windowHeight);
        mouseX = x * mc->width() / windowWidth;
        mouseY = y * mc->height() / windowHeight;
        return true;
    }
    [[nodiscard]] bool blocksWorldUpdate() const override { return true; }
    [[nodiscard]] bool blocksWorldRender() const override { return true; }
    void tick() const override {
        if (cancelled.load()) return;
        switch (result.load(std::memory_order_acquire)) {
            case Result::Connected:
                if (mc->netClient != nullptr && mc->netClient->hasFailed()) {
                    const std::string reason = mc->netClient->failureReason();
                    mc->netClient->disconnect();
                    mc->clearScheduledTasks();
                    delete mc->netClient;
                    mc->netClient = nullptr;
                    result.store(Result::Handled, std::memory_order_release);
                    mc->displayGuiScreen(new GuiDisconnected(parent, "Cannot connect to server",
                        reason.empty() ? "Connection closed by the server" : reason));
                    return;
                }
                if (mc->world != nullptr) {
                    result.store(Result::Handled, std::memory_order_release);
                    mc->displayGuiScreen(new GuiDownloadTerrain());
                }
                return;
            case Result::Failed:
                result.store(Result::Handled, std::memory_order_release);
                mc->displayGuiScreen(new GuiDisconnected(parent, "Cannot connect to server",
                    "Failed to connect to the server"));
                return;
            case Result::Pending:
            case Result::Handled:
                return;
        }
    }

private:
    enum class Result : uint8_t { Pending, Connected, Failed, Handled };
    Gui* parent;
    std::string host;
    uint16_t port;
    mutable std::atomic<Result> result{Result::Pending};
    mutable std::atomic_bool started{false};
    mutable std::atomic_bool cancelled{false};
    mutable double mouseX = -1.0;
    mutable double mouseY = -1.0;
    static inline GLuint backgroundTexture = 0;
    static inline GLuint widgetsTexture = 0;

    [[nodiscard]] int scaleFactor() const {
        int factor = 1;
        const int requested = mc->settings->guiScale == 0 ? 1000 : mc->settings->guiScale;
        while (factor < requested && mc->width() / (factor + 1) >= 320
            && mc->height() / (factor + 1) >= 240) ++factor;
        return factor;
    }
    void drawDirt() const {
        Gui::render();
        const float tile = 32.0f * scaleFactor();
        for (float y = 0; y < mc->height(); y += tile) for (float x = 0; x < mc->width(); x += tile)
            drawTexture(backgroundTexture, x, y, tile, tile, 0, 0, 32, 32, 32, 32,
                {0.38f, 0.38f, 0.38f, 1.0f});
    }
    [[nodiscard]] bool buttonHovered() const {
        const float scale = static_cast<float>(scaleFactor());
        const float x = mc->width() * 0.5f - 100.0f * scale;
        const float y = mc->height() * 0.25f + 132.0f * scale;
        return mouseX >= x && mouseX < x + 200.0f * scale
            && mouseY >= y && mouseY < y + 20.0f * scale;
    }
    void drawButton() const {
        const float scale = static_cast<float>(scaleFactor());
        const float x = mc->width() * 0.5f - 100.0f * scale;
        const float y = mc->height() * 0.25f + 132.0f * scale;
        const float textureY = buttonHovered() ? 86.0f : 66.0f;
        drawTexture(widgetsTexture, x, y, 100.0f * scale, 20.0f * scale,
            0, textureY, 256, 256, 100, 20);
        drawTexture(widgetsTexture, x + 100.0f * scale, y, 100.0f * scale, 20.0f * scale,
            100, textureY, 256, 256, 100, 20);
        const char* label = "Cancel";
        mc->fontRenderer->renderText(label,
            mc->width() * 0.5f - mc->fontRenderer->getStringWidth(label) * scale * 0.5f,
            y + 6.0f * scale, scale, buttonHovered() ? 0xFFFFFFA0 : 0xFFE0E0E0, true);
    }
};
