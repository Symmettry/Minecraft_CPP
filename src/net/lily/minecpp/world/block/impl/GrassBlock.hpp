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

    [[nodiscard]] std::string getTextureName(const int face) const override {
        switch (face) {
            case 4: return "grass_top";
            case 5: return "dirt";
            default: return "grass_side";
        }
    }
};


#endif