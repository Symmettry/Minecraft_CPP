//
// Created by lily on 12/27/25.
//

#ifndef MINECRAFTCLIENT_MAINMENU_HPP
#define MINECRAFTCLIENT_MAINMENU_HPP
#include "net/lily/minecpp/menu/Gui.hpp"
#include "net/lily/minecpp/Minecraft.hpp"

class GuiMainMenu : public Gui {
public:
    void onClose() override {

    }

    void update(float deltaSeconds) override {

    }
    void render() const override {
        Gui::render();

        drawRect(0, 0, mc->width(), mc->height(), {1.0f, 1.0f, 1.0f, 1.0f});
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
        return false;
    }
    [[nodiscard]] bool blocksWorldRender() const override {
        return false;
    }

    void tick() const override {

    }
};

#endif //MINECRAFTCLIENT_MAINMENU_HPP