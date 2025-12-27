#include "Minecraft.hpp"

const static Minecraft* MineInst = new Minecraft();

const Minecraft* getMinecraft() {
    return MineInst;
}

int main() {
    return 0;
}