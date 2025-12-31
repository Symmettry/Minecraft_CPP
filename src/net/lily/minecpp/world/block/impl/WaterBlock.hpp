//
// Created by lily on 12/30/25.
//

#ifndef MINECRAFTCLIENT_WATERBLOCK_HPP
#define MINECRAFTCLIENT_WATERBLOCK_HPP
#include "net/lily/minecpp/world/block/Block.hpp"


class WaterBlock : public Block {
public:
    WaterBlock(const int x, const int y, const int z) : Block(Material::Water, x, y, z) {}

    [[nodiscard]] std::string getName() const override {
        return "Water";
    }

    [[nodiscard]] bool isOpaque() const override {
        return false;
    }

    void onPlace() override {}
    void onBreak() override {}

    [[nodiscard]] std::string getTextureName(int face) const override {
        return "water_still";
    }
};

#endif //MINECRAFTCLIENT_WATERBLOCK_HPP