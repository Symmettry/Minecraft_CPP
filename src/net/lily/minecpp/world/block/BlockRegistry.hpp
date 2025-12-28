#pragma once

#include <unordered_map>
#include <functional>
#include <memory>

#include "Block.hpp"
#include "impl/AirBlock.hpp"
#include "impl/DirtBlock.hpp"
#include "impl/GrassBlock.hpp"

using BlockFactory = std::function<std::unique_ptr<Block>(int, int, int)>;

class BlockRegistry {
public:

    template <typename BlockType>
    static void registerDefBlockHelper(Material material) {
        registerBlock(material, [](int x, int y, int z) {
            return std::make_unique<BlockType>(x, y, z);
        });
    }

    static void initialize() {
        registerDefBlockHelper<GrassBlock>(Material::Grass);
        registerDefBlockHelper<DirtBlock>(Material::Dirt);
        registerDefBlockHelper<AirBlock>(Material::Air);
    }

    static void registerBlock(const Material mat, const BlockFactory &factory) {
        getRegistry()[mat] = factory;
    }

    static std::unique_ptr<Block> createBlock(const Material mat, const int x, const int y, const int z) {
        if (const auto it = getRegistry().find(mat); it != getRegistry().end()) {
            return it->second(x, y, z);
        }
        throw std::runtime_error("No factory for material " + std::to_string(static_cast<int>(mat)));
    }

private:
    static std::unordered_map<Material, BlockFactory>& getRegistry() {
        static std::unordered_map<Material, BlockFactory> registry;
        return registry;
    }
};
