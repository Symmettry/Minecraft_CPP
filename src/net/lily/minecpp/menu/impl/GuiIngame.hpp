//
// Created by lily on 12/27/25.
//

#ifndef MINECRAFTCLIENT_GUIINGAME_HPP
#define MINECRAFTCLIENT_GUIINGAME_HPP
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/menu/Gui.hpp"
#include "net/lily/minecpp/net/packets/play/NetHandlerPlay.hpp"
#include "net/lily/minecpp/util/ChatHistory.hpp"

class GuiIngame : public Gui {

    std::string chatHistory[256];

public:
    void onOpen() override {
        Gui::onOpen();
    }
    void onClose() override {

    }

    void update(float deltaSeconds) override {

    }
    void render() const override {
        mc->renderer->render(mc->world);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);

        const FontRenderer* fr = mc->fontRenderer;
        fr->shader->use();
        fr->shader->setMat4("projection", glm::value_ptr(glm::ortho(0.0f, static_cast<float>(mc->width()), static_cast<float>(mc->height()), 0.0f)));

        fr->renderText(mc->player->coordinates(), 10, 10, 2.0f, 0xFFFFFFFF, true);
        fr->renderText(mc->player->rotations(), 10, 30, 2.0f, 0xFFFFFFFF, true);
        fr->renderText("FPS: " + std::to_string(mc->renderer->fps), 10, 50, 2.0f, 0xFFFFFFFF, true);

        int i=0;
        for (const auto &[fst, snd] : ChatHistory::chatHistory) {
            fr->renderText(fst, 5, static_cast<float>(mc->height() - 20 - (ChatHistory::chatHistory.size() - i - 1) * 20), 2, 0xFFFFFFFF, true);
            i++;
        }

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        glfwSwapBuffers(mc->renderer->window);
        glfwPollEvents();
    }

    bool handleKey(int key, int scancode, int action, int mods) override {
        return true;
    }
    bool handleMouseButton(int button, int action, int mods) override {
        return true;
    }

    bool handleCursorPos(double x, double y) override {
        return true;
    }

    [[nodiscard]] bool blocksWorldUpdate() const override {
        return true;
    }
    [[nodiscard]] bool blocksWorldRender() const override {
        return true;
    }

    void tick() const override {
        if (!mc->world || !mc->netClient->isHandlerDone()) return;
        mc->input->updateMouse();
        mc->world->update();
    }

};

#endif //MINECRAFTCLIENT_GUIINGAME_HPP