#include "menu/impl/GuiMainMenu.hpp"
#include "menu/Gui.hpp"

#include "Minecraft.hpp"

#include "menu/impl/GuiIngame.hpp"

const static Minecraft* MineInst;
static GuiIngame* ingameGui;

const Minecraft* Minecraft::getMinecraft() {
    return MineInst;
}

void Minecraft::render() const {
    if (currentScreen) currentScreen->render();
}

void Minecraft::runTick() const {
    if (currentScreen) currentScreen->tick();
}

void Minecraft::displayGuiScreen(Gui* gui) const {
    if (!gui) {
        if (world == nullptr) {
            printf("Going to main menu");
            gui = new GuiMainMenu();
        } else if (player->health <= 0.0f) {
            // <set to death screen> todo
            printf("Blehhhh\n");
        } else {
            if (!ingameGui) ingameGui = new GuiIngame();
            gui = ingameGui;
            printf("Going to in game gui\n");
        }
    }

    if (currentScreen != nullptr && currentScreen != gui) {
        currentScreen->onClose();
    }

    currentScreen = gui;
    if (currentScreen) currentScreen->onOpen();
}

int main() {
    MineInst = new Minecraft("127.0.0.1");
    Gui::init();
    MineInst->displayGuiScreen(nullptr);
    MineInst->init();
    return 0;
}