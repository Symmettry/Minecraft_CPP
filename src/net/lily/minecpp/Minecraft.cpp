#include "menu/impl/GuiMainMenu.hpp"
#include "menu/Gui.hpp"

#include "Minecraft.hpp"

#include "menu/impl/GuiIngame.hpp"

const static Minecraft* MineInst;

const Minecraft* Minecraft::getMinecraft() {
    return MineInst;
}

void Minecraft::render() const {
    currentScreen->render();
}

void Minecraft::runTick() const {
    currentScreen->tick();
}

int main() {
    MineInst = new Minecraft("127.0.0.1");
    Gui::init();
    MineInst->displayGuiScreen(new GuiIngame());
    MineInst->init();
    return 0;
}