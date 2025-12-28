#ifndef MINECRAFTCLIENT_GRASSBLOCK_HPP
#define MINECRAFTCLIENT_GRASSBLOCK_HPP

class GrassBlock : public Block {
public:
    GrassBlock(const int x, const int y, const int z) : Block(Material::Grass, x, y, z) {}

    [[nodiscard]] std::string getName() const override {
        return "Grass";
    }

    [[nodiscard]] bool isOpaque() const override {
        return true;
    }

    void onPlace() override {}
    void onBreak() override {}
};


#endif