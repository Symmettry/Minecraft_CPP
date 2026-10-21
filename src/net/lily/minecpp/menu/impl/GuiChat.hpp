#pragma once

#include <algorithm>
#include <string>

#include "GuiIngame.hpp"
#include "net/lily/minecpp/net/packets/play/client/C01PacketChatMessage.hpp"
#include "net/lily/minecpp/util/Utf8.hpp"

class GuiChat final : public Gui {
public:
    explicit GuiChat(GuiIngame* parent, std::string initial = {})
        : parent(parent), input(std::move(initial)) {}

    void onOpen() override {
        parent->setChatOpen(true);
        mc->settings->inputEnabled = false;
        glfwSetInputMode(mc->renderer->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        historyPosition = static_cast<int>(ChatHistory::sentMessages.size());
    }

    void onClose() override {
        parent->setChatOpen(false);
        mc->settings->inputEnabled = true;
    }

    void update(float) override {}

    void render() const override {
        parent->render();
        const int factor = scaleFactor();
        const float scale = static_cast<float>(factor);
        const float y = static_cast<float>(mc->height()) - 14.0f * scale;
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        drawRect(2.0f * scale, y - 2.0f * scale, static_cast<float>(mc->width()) - 4.0f * scale,
            12.0f * scale, {0.0f, 0.0f, 0.0f, 0.5f});
        const std::string visible = input + (cursorVisible ? "_" : "");
        mc->fontRenderer->renderText(visible, 4.0f * scale, y, scale, 0xFFFFFFFF, true);
    }

    bool handleKey(const int key, int, const int action, const int mods) override {
        if (action != GLFW_PRESS && action != GLFW_REPEAT) return true;
        switch (key) {
            case GLFW_KEY_ESCAPE:
                mc->displayGuiScreen(parent);
                break;
            case GLFW_KEY_ENTER:
            case GLFW_KEY_KP_ENTER:
                sendMessage();
                break;
            case GLFW_KEY_BACKSPACE:
                Utf8::popBack(input);
                break;
            case GLFW_KEY_UP:
                history(-1);
                break;
            case GLFW_KEY_DOWN:
                history(1);
                break;
            case GLFW_KEY_V:
                if ((mods & GLFW_MOD_CONTROL) != 0) {
                    if (const char* clipboard = glfwGetClipboardString(mc->renderer->window)) {
                        input += clipboard;
                        Utf8::truncate(input, 100);
                    }
                }
                break;
            default:
                break;
        }
        return true;
    }

    bool handleChar(const unsigned int codepoint) override {
        if (suppressOpeningCharacter) {
            suppressOpeningCharacter = false;
            return true;
        }
        if (codepoint >= 32 && codepoint != 127 && Utf8::length(input) < 100) {
            Utf8::append(input, codepoint);
        }
        return true;
    }
    bool handleMouseButton(int, int, int) override { return true; }
    bool handleCursorPos(double, double) override { return true; }
    [[nodiscard]] bool blocksWorldUpdate() const override { return true; }
    [[nodiscard]] bool blocksWorldRender() const override { return true; }
    [[nodiscard]] bool pausesGame() const override { return false; }
    void tick() const override {
        ++cursorTicks;
        cursorVisible = (cursorTicks / 6) % 2 == 0;
        if (mc->world != nullptr && mc->netClient != nullptr && mc->netClient->isHandlerDone()) mc->world->update();
    }

private:
    GuiIngame* parent;
    std::string input;
    std::string draft;
    int historyPosition = 0;
    mutable int cursorTicks = 0;
    mutable bool cursorVisible = true;
    bool suppressOpeningCharacter = true;

    [[nodiscard]] int scaleFactor() const {
        int factor = 1;
        const int requested = mc->settings->guiScale == 0 ? 1000 : mc->settings->guiScale;
        while (factor < requested && mc->width() / (factor + 1) >= 320
               && mc->height() / (factor + 1) >= 240) ++factor;
        return factor;
    }

    void sendMessage() const {
        if (!input.empty()) {
            ChatHistory::addSentMessage(input);
            if (mc->netClient != nullptr) mc->netClient->sendPacket(C01PacketChatMessage{input});
        }
        mc->displayGuiScreen(parent);
    }

    void history(const int direction) {
        const int size = static_cast<int>(ChatHistory::sentMessages.size());
        if (historyPosition == size) draft = input;
        historyPosition = std::clamp(historyPosition + direction, 0, size);
        input = historyPosition == size ? draft : ChatHistory::sentMessages[historyPosition];
    }
};
