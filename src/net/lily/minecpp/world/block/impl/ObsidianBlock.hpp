//
// Created by lily on 12/27/25.
//

#ifndef MINECRAFTCLIENT_OBSIDIANBLOCK_HPP
#define MINECRAFTCLIENT_OBSIDIANBLOCK_HPP
#include "net/lily/minecpp/world/block/Block.hpp"

class ObsidianBlock : public Block {
public:
    ObsidianBlock(const int x, const int y, const int z) : Block(Material::Obsidian, x, y, z) {}

    [[nodiscard]] std::string getName() const override {
        return "Obsidian";
    }

    [[nodiscard]] bool isOpaque() const override {
        return true;
    }

    void onPlace() override {}
    void onBreak() override {}

    [[nodiscard]] std::string getTextureName(int face) const override {
        return "obsidian";
    }
};

#endif