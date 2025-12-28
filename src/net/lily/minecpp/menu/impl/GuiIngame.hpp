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

class GuiIngame : public Gui {

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
        mc->input->updateMouse();
        mc->world->update();
        if (mc->netClient) mc->netClient->tick();
    }

};

#endif //MINECRAFTCLIENT_GUIINGAME_HPP