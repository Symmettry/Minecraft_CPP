//
// Created by lily on 12/27/25.
//

#ifndef MINECRAFTCLIENT_AIRBLOCK_HPP
#define MINECRAFTCLIENT_AIRBLOCK_HPP
#include "net/lily/minecpp/world/block/Block.hpp"

class AirBlock : public Block {
public:
    AirBlock(const int x, const int y, const int z) : Block(x, y, z) {}

    [[nodiscard]] std::string getName() const override {
        return "Air";
    }

    [[nodiscard]] bool isOpaque() const override {
        return false;
    }

    void onPlace() override {}
    void onBreak() override {}

};

#endif //MINECRAFTCLIENT_AIRBLOCK_HPP