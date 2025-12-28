//
// Created by lily on 12/27/25.
//

#ifndef MINECRAFTCLIENT_ICEBLOCK_HPP
#define MINECRAFTCLIENT_ICEBLOCK_HPP
#include "net/lily/minecpp/world/block/Block.hpp"

class IceBlock : public Block {
public:
    IceBlock(const int x, const int y, const int z) : Block(Material::Ice, x, y, z) {}

    [[nodiscard]] std::string getName() const override {
        return "Ice";
    }

    [[nodiscard]] bool isOpaque() const override {
        return true;
    }

    void onPlace() override {}
    void onBreak() override {}

    [[nodiscard]] std::string getTextureName(int face) const override {
        return "ice";
    }

    [[nodiscard]] float slipperiness() const override {
        return 0.98f;
    }

};

#endif