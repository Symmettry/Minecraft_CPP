//
// Created by lily on 12/27/25.
//

#ifndef MINECRAFTCLIENT_DIRTBLOCK_HPP
#define MINECRAFTCLIENT_DIRTBLOCK_HPP
#include "net/lily/minecpp/world/block/Block.hpp"

class DirtBlock : public Block {
public:
    DirtBlock(const int x, const int y, const int z) : Block(Material::Dirt, x, y, z) {}

    [[nodiscard]] std::string getName() const override {
        return "Dirt";
    }

    [[nodiscard]] bool isOpaque() const override {
        return true;
    }

    void onPlace() override {}
    void onBreak() override {}
};


#endif //MINECRAFTCLIENT_DIRTBLOCK_HPP