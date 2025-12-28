//
// Created by lily on 12/27/25.
//

#ifndef MINECRAFTCLIENT_STONEBLOCK_HPP
#define MINECRAFTCLIENT_STONEBLOCK_HPP
#include "net/lily/minecpp/world/block/Block.hpp"

class StoneBlock : public Block {
public:
    StoneBlock(const int x, const int y, const int z) : Block(Material::Stone, x, y, z) {}

    [[nodiscard]] std::string getName() const override {
        return "Stone";
    }

    [[nodiscard]] bool isOpaque() const override {
        return true;
    }

    void onPlace() override {}
    void onBreak() override {}

    [[nodiscard]] std::string getTextureName(int face) const override {
        return "stone";
    }
};

#endif //MINECRAFTCLIENT_STONEBLOCK_HPP