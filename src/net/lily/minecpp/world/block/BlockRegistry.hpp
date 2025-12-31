#pragma once

#include <unordered_map>
#include <functional>
#include <memory>

#include "Block.hpp"
#include "impl/AirBlock.hpp"
#include "impl/DirtBlock.hpp"
#include "impl/GrassBlock.hpp"
#include "impl/IceBlock.hpp"
#include "impl/StoneBlock.hpp"
#include "impl/ObsidianBlock.hpp"
#include "impl/WaterBlock.hpp"

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
        registerDefBlockHelper<StoneBlock>(Material::Stone);
        registerDefBlockHelper<IceBlock>(Material::Ice);
        registerDefBlockHelper<ObsidianBlock>(Material::Obsidian);
        registerDefBlockHelper<WaterBlock>(Material::Water);
    }

    static void registerBlock(const Material mat, const BlockFactory &factory) {
        getRegistry()[mat] = factory;
    }

    static std::unique_ptr<Block> createBlock(const Material mat, const int x, const int y, const int z, const bool unsafe = false) {
        if (const auto it = getRegistry().find(mat); it != getRegistry().end()) {
            return it->second(x, y, z);
        }
        std::cerr << "No factory for material " << std::to_string(static_cast<int>(mat)) << std::endl;
        if (unsafe) return createBlock(Material::Dirt, x, y, z);
        throw std::runtime_error("No factory for material " + std::to_string(static_cast<int>(mat)));
    }

    static Material matFromId(const int id) {
        return static_cast<Material>(id);
    }

private:
    static std::unordered_map<Material, BlockFactory>& getRegistry() {
        static std::unordered_map<Material, BlockFactory> registry;
        return registry;
    }
};
