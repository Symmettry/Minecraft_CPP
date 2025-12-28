#include "Minecraft.hpp"

const static Minecraft* MineInst = new Minecraft("127.0.0.1");

const Minecraft* getMinecraft() {
    return MineInst;
}

int main() {
    return 0;
}