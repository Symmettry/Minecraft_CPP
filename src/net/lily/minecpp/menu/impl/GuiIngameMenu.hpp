#pragma once

#include <array>
#include <string>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/menu/Gui.hpp"
#include "net/lily/minecpp/menu/impl/GuiMultiplayer.hpp"
#include "net/lily/minecpp/menu/impl/GuiOptions.hpp"

class GuiIngameMenu final : public Gui {
    enum class Action { ReturnToGame, Achievements, Statistics, Options, OpenToLan, Disconnect };

    struct Bounds { float x; float y; float width; float height; };
    struct Button { const char* label; Action action; bool enabled; };

    static constexpr std::array buttons{
        Button{"Return to Game", Action::ReturnToGame, true},
        Button{"Achievements", Action::Achievements, false},
        Button{"Statistics", Action::Statistics, false},
        Button{"Options...", Action::Options, true},
        Button{"Open to LAN", Action::OpenToLan, false},
        Button{"Disconnect", Action::Disconnect, true}
    };

    mutable double mouseX = -1.0;
    mutable double mouseY = -1.0;
    static inline GLuint widgetsTexture = 0;

    [[nodiscard]] int scaleFactor() const {
        int factor = 1;
        const int requested = mc->settings->guiScale == 0 ? 1000 : mc->settings->guiScale;
        while (factor < requested && mc->width() / (factor + 1) >= 320
            && mc->height() / (factor + 1) >= 240) ++factor;
        return factor;
    }

    [[nodiscard]] int width() const { const int factor = scaleFactor(); return (mc->width() + factor - 1) / factor; }
    [[nodiscard]] int height() const { const int factor = scaleFactor(); return (mc->height() + factor - 1) / factor; }

    [[nodiscard]] Bounds buttonBounds(const size_t index) const {
        const float center = static_cast<float>(width()) * 0.5f;
        const float base = static_cast<float>(height()) / 4.0f - 16.0f;
        switch (index) {
            case 0: return {center - 100.0f, base + 24.0f, 200.0f, 20.0f};
            case 1: return {center - 100.0f, base + 48.0f, 98.0f, 20.0f};
            case 2: return {center + 2.0f, base + 48.0f, 98.0f, 20.0f};
            case 3: return {center - 100.0f, base + 96.0f, 98.0f, 20.0f};
            case 4: return {center + 2.0f, base + 96.0f, 98.0f, 20.0f};
            case 5: return {center - 100.0f, base + 120.0f, 200.0f, 20.0f};
            default: return {};
        }
    }

    [[nodiscard]] bool hovered(const Bounds& bounds) const {
        return mouseX >= bounds.x && mouseX < bounds.x + bounds.width
            && mouseY >= bounds.y && mouseY < bounds.y + bounds.height;
    }

    static void drawCentered(const std::string& text, const float centerX, const float y,
                             const float scale, const unsigned int color) {
        mc->fontRenderer->renderText(text,
            centerX - static_cast<float>(mc->fontRenderer->getStringWidth(text)) * scale * 0.5f,
            y, scale, color, true);
    }

    void drawButton(const size_t index) const {
        const Button& button = buttons[index];
        const Bounds bounds = buttonBounds(index);
        const float scale = static_cast<float>(scaleFactor());
        const bool highlighted = button.enabled && hovered(bounds);
        const float textureY = !button.enabled ? 46.0f : highlighted ? 86.0f : 66.0f;
        const float half = bounds.width * 0.5f;
        drawTexture(widgetsTexture, bounds.x * scale, bounds.y * scale,
            half * scale, bounds.height * scale, 0, textureY, 256, 256, half, bounds.height);
        drawTexture(widgetsTexture, (bounds.x + half) * scale, bounds.y * scale,
            half * scale, bounds.height * scale, 200 - half, textureY, 256, 256, half, bounds.height);
        drawCentered(button.label, (bounds.x + bounds.width * 0.5f) * scale,
            (bounds.y + 6.0f) * scale, scale,
            !button.enabled ? 0xFFA0A0A0 : highlighted ? 0xFFFFFFA0 : 0xFFE0E0E0);
    }

    void activate(const Action action) const {
        switch (action) {
            case Action::ReturnToGame:
                mc->displayGuiScreen(nullptr);
                break;
            case Action::Options:
                mc->displayGuiScreen(new GuiOptions(const_cast<GuiIngameMenu*>(this)));
                break;
            case Action::Disconnect:
                if (mc->netClient != nullptr) {
                    mc->netClient->disconnect();
                    mc->clearScheduledTasks();
                    delete mc->netClient;
                    mc->netClient = nullptr;
                }
                mc->world = nullptr;
                mc->displayGuiScreen(new GuiMultiplayer());
                break;
            case Action::Achievements:
            case Action::Statistics:
            case Action::OpenToLan:
                break;
        }
    }

public:
    void onOpen() override {
        if (widgetsTexture == 0) widgetsTexture = loadTexture("assets/minecraft/textures/gui/widgets.png");
        mc->settings->inputEnabled = false;
        if (mc->isSingleplayer()) mc->timer->partialTicks = 1.0;
        glfwSetInputMode(mc->renderer->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    void onClose() override {}
    void update(float) override {}

    void render() const override {
        mc->renderer->render(mc->world);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        constexpr int strips = 32;
        for (int strip = 0; strip < strips; ++strip) {
            const float top = static_cast<float>(mc->height()) * strip / strips;
            const float bottom = static_cast<float>(mc->height()) * (strip + 1) / strips;
            const float alpha = 0.75f + 0.0625f * static_cast<float>(strip) / static_cast<float>(strips - 1);
            drawRect(0.0f, top, static_cast<float>(mc->width()), bottom - top, {0.063f, 0.063f, 0.063f, alpha});
        }

        const float scale = static_cast<float>(scaleFactor());
        mc->fontRenderer->shader->use();
        mc->fontRenderer->shader->setMat4("projection", glm::value_ptr(glm::ortho(
            0.0f, static_cast<float>(mc->width()), static_cast<float>(mc->height()), 0.0f)));
        drawCentered("Game menu", static_cast<float>(width()) * scale * 0.5f, 40.0f * scale, scale, 0xFFFFFFFF);
        for (size_t index = 0; index < buttons.size(); ++index) drawButton(index);
    }

    bool handleKey(const int key, int, const int action, int) override {
        if (key != GLFW_KEY_ESCAPE || action != GLFW_PRESS) return false;
        activate(Action::ReturnToGame);
        return true;
    }
    bool handleMouseButton(const int button, const int action, int) override {
        if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return false;
        for (size_t index = 0; index < buttons.size(); ++index) {
            if (!buttons[index].enabled || !hovered(buttonBounds(index))) continue;
            activate(buttons[index].action);
            return true;
        }
        return false;
    }
    bool handleCursorPos(const double x, const double y) override {
        int windowWidth = 1;
        int windowHeight = 1;
        glfwGetWindowSize(mc->renderer->window, &windowWidth, &windowHeight);
        mouseX = x * mc->width() / static_cast<double>(windowWidth) / scaleFactor();
        mouseY = y * mc->height() / static_cast<double>(windowHeight) / scaleFactor();
        return true;
    }
    [[nodiscard]] bool blocksWorldUpdate() const override { return mc->isSingleplayer(); }
    [[nodiscard]] bool blocksWorldRender() const override { return false; }
    [[nodiscard]] bool pausesGame() const override { return mc->isSingleplayer(); }
    void tick() const override {
        if (mc->isSingleplayer() || mc->world == nullptr || mc->netClient == nullptr
            || !mc->netClient->isHandlerDone()) return;
        mc->world->update();
    }
};
