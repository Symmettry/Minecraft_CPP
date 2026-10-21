#pragma once

#include <array>
#include <algorithm>
#include <string>
#include <utility>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/menu/Gui.hpp"
#include "net/lily/minecpp/menu/impl/GuiMainMenu.hpp"
#include "net/lily/minecpp/net/packets/play/client/MissingPlayPackets.hpp"

class GuiGameOver final : public Gui {
    enum class Action { Respawn, TitleScreen };
    struct Bounds { float x; float y; float width; float height; };
    struct Button { const char* label; Action action; };

    static constexpr std::array buttons{
        Button{"Respawn", Action::Respawn},
        Button{"Title screen", Action::TitleScreen}
    };

    mutable double mouseX = -1.0;
    mutable double mouseY = -1.0;
    mutable int ticksOpen = 0;
    mutable bool waitingForRespawn = false;
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
        return {static_cast<float>(width()) * 0.5f - 100.0f,
            static_cast<float>(height()) / 4.0f + 72.0f + static_cast<float>(index) * 24.0f,
            200.0f, 20.0f};
    }
    [[nodiscard]] bool hovered(const Bounds& bounds) const {
        return mouseX >= bounds.x && mouseX < bounds.x + bounds.width
            && mouseY >= bounds.y && mouseY < bounds.y + bounds.height;
    }
    static void centeredText(const std::string& text, const float centerX, const float y,
                             const float scale, const unsigned int color) {
        mc->fontRenderer->renderText(text,
            centerX - static_cast<float>(mc->fontRenderer->getStringWidth(text)) * scale * 0.5f,
            y, scale, color, true);
    }
    void drawButton(const size_t index) const {
        const Bounds bounds = buttonBounds(index);
        const float scale = static_cast<float>(scaleFactor());
        const bool enabled = ticksOpen >= 20 && !(index == 0 && waitingForRespawn);
        const bool highlighted = enabled && hovered(bounds);
        const float textureY = !enabled ? 46.0f : highlighted ? 86.0f : 66.0f;
        const float half = bounds.width * 0.5f;
        drawTexture(widgetsTexture, bounds.x * scale, bounds.y * scale,
            half * scale, 20.0f * scale, 0, textureY, 256, 256, half, 20);
        drawTexture(widgetsTexture, (bounds.x + half) * scale, bounds.y * scale,
            half * scale, 20.0f * scale, 200 - half, textureY, 256, 256, half, 20);
        const std::string label = index == 0 && waitingForRespawn ? "Respawning..." : buttons[index].label;
        centeredText(label, (bounds.x + bounds.width * 0.5f) * scale, (bounds.y + 6.0f) * scale,
            scale, !enabled ? 0xFFA0A0A0 : highlighted ? 0xFFFFFFA0 : 0xFFE0E0E0);
    }
    void activate(const Action action) const {
        switch (action) {
            case Action::Respawn: {
                if (mc->netClient == nullptr || waitingForRespawn) return;
                PacketBuffer payload;
                payload.writeVarInt(0);
                mc->netClient->sendPacket(C16PacketClientStatus{std::move(payload)});
                waitingForRespawn = true;
                mc->player->health = 20.0f;
                mc->displayGuiScreen(nullptr);
                break;
            }
            case Action::TitleScreen:
                if (mc->netClient != nullptr) {
                    mc->netClient->disconnect();
                    mc->clearScheduledTasks();
                    delete mc->netClient;
                    mc->netClient = nullptr;
                }
                mc->world = nullptr;
                mc->displayGuiScreen(new GuiMainMenu());
                break;
        }
    }

public:
    void onOpen() override {
        if (widgetsTexture == 0) widgetsTexture = loadTexture("assets/minecraft/textures/gui/widgets.png");
        mc->settings->inputEnabled = false;
        mc->input->recenterMouse();
        glfwSetInputMode(mc->renderer->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    void onClose() override {}
    void update(float) override {}
    void render() const override {
        if (mc->world != nullptr) mc->renderer->render(mc->world);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        constexpr glm::vec4 topColor{80.0f / 255.0f, 0.0f, 0.0f, 96.0f / 255.0f};
        constexpr glm::vec4 bottomColor{128.0f / 255.0f, 48.0f / 255.0f, 48.0f / 255.0f, 160.0f / 255.0f};
        const int logicalHeight = std::max(height(), 1);
        const float physicalScale = static_cast<float>(scaleFactor());
        for (int y = 0; y < logicalHeight; ++y) {
            const float amount = static_cast<float>(y) / static_cast<float>(logicalHeight);
            drawRect(0.0f, static_cast<float>(y) * physicalScale,
                static_cast<float>(mc->width()), physicalScale + 1.0f,
                topColor + (bottomColor - topColor) * amount);
        }

        const float scale = physicalScale;
        mc->fontRenderer->shader->use();
        mc->fontRenderer->shader->setMat4("projection", glm::value_ptr(glm::ortho(
            0.0f, static_cast<float>(mc->width()), static_cast<float>(mc->height()), 0.0f)));
        centeredText("You died!", static_cast<float>(width()) * scale * 0.5f,
            60.0f * scale, 2.0f * scale, 0xFFFFFFFF);

        const std::string scoreLabel = "Score: ";
        const std::string score = "0";
        const float scoreWidth = static_cast<float>(mc->fontRenderer->getStringWidth(scoreLabel + score)) * scale;
        const float scoreX = static_cast<float>(width()) * scale * 0.5f - scoreWidth * 0.5f;
        mc->fontRenderer->renderText(scoreLabel, scoreX, 100.0f * scale, scale, 0xFFFFFFFF, true);
        mc->fontRenderer->renderText(score,
            scoreX + static_cast<float>(mc->fontRenderer->getStringWidth(scoreLabel)) * scale,
            100.0f * scale, scale, 0xFFFFFF00, true);
        for (size_t index = 0; index < buttons.size(); ++index) drawButton(index);
    }
    bool handleKey(int, int, int, int) override { return true; }
    bool handleMouseButton(const int button, const int action, int) override {
        if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS || ticksOpen < 20) return false;
        for (size_t index = 0; index < buttons.size(); ++index) {
            if (!hovered(buttonBounds(index)) || (index == 0 && waitingForRespawn)) continue;
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
    [[nodiscard]] bool blocksWorldUpdate() const override { return false; }
    [[nodiscard]] bool blocksWorldRender() const override { return false; }
    void tick() const override {
        ++ticksOpen;
        if (mc->world != nullptr && (mc->netClient == nullptr || mc->netClient->isHandlerDone())) mc->world->update();
    }
};
