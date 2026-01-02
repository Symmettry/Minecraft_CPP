#pragma once
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <array>
#include <iostream>
#include <ranges>

#include "net/lily/minecpp/util/AABB.hpp"
#include "net/lily/minecpp/util/Zstd.hpp"

#define F false
#define N nullptr
#define SC 0.6f

using Block = uint16_t;

constexpr Block
    BLOCK_AIR                     = 0 << 4
  , BLOCK_STONE                   = 1 << 4
  , BLOCK_GRASS                   = 2 << 4
  , BLOCK_DIRT                    = 3 << 4
  , BLOCK_COBBLESTONE             = 4 << 4
  , BLOCK_PLANKS                  = 5 << 4
  , BLOCK_SAPLING                 = 6 << 4
  , BLOCK_BEDROCK                 = 7 << 4
  , BLOCK_FLOWING_WATER           = 8 << 4
  , BLOCK_WATER                   = 9 << 4
  , BLOCK_FLOWING_LAVA            = 10 << 4
  , BLOCK_LAVA                    = 11 << 4
  , BLOCK_SAND                    = 12 << 4
  , BLOCK_GRAVEL                  = 13 << 4
  , BLOCK_GOLD_ORE                = 14 << 4
  , BLOCK_IRON_ORE                = 15 << 4
  , BLOCK_COAL_ORE                = 16 << 4
  , BLOCK_LOG                     = 17 << 4
  , BLOCK_LEAVES                  = 18 << 4
  , BLOCK_SPONGE                  = 19 << 4
  , BLOCK_GLASS                   = 20 << 4
  , BLOCK_LAPIS_ORE               = 21 << 4
  , BLOCK_LAPIS_BLOCK             = 22 << 4
  , BLOCK_DISPENSER               = 23 << 4
  , BLOCK_SANDSTONE               = 24 << 4
  , BLOCK_NOTEBLOCK               = 25 << 4
  , BLOCK_BED                     = 26 << 4
  , BLOCK_GOLDEN_RAIL             = 27 << 4
  , BLOCK_DETECTOR_RAIL           = 28 << 4
  , BLOCK_STICKY_PISTON           = 29 << 4
  , BLOCK_WEB                     = 30 << 4
  , BLOCK_TALLGRASS               = 31 << 4
  , BLOCK_DEADBUSH                = 32 << 4
  , BLOCK_PISTON                  = 33 << 4
  , BLOCK_PISTON_HEAD             = 34 << 4
  , BLOCK_WOOL                    = 35 << 4
  , BLOCK_PISTON_EXTENSION        = 36 << 4
  , BLOCK_YELLOW_FLOWER           = 37 << 4
  , BLOCK_RED_FLOWER              = 38 << 4
  , BLOCK_BROWN_MUSHROOM          = 39 << 4
  , BLOCK_RED_MUSHROOM            = 40 << 4
  , BLOCK_GOLD_BLOCK              = 41 << 4
  , BLOCK_IRON_BLOCK              = 42 << 4
  , BLOCK_DOUBLE_STONE_SLAB       = 43 << 4
  , BLOCK_STONE_SLAB              = 44 << 4
  , BLOCK_BRICK_BLOCK             = 45 << 4
  , BLOCK_TNT                     = 46 << 4
  , BLOCK_BOOKSHELF               = 47 << 4
  , BLOCK_MOSSY_COBBLESTONE       = 48 << 4
  , BLOCK_OBSIDIAN                = 49 << 4
  , BLOCK_TORCH                   = 50 << 4
  , BLOCK_FIRE                    = 51 << 4
  , BLOCK_MOB_SPAWNER             = 52 << 4
  , BLOCK_OAK_STAIRS              = 53 << 4
  , BLOCK_CHEST                   = 54 << 4
  , BLOCK_REDSTONE_WIRE           = 55 << 4
  , BLOCK_DIAMOND_ORE             = 56 << 4
  , BLOCK_DIAMOND_BLOCK           = 57 << 4
  , BLOCK_CRAFTING_TABLE          = 58 << 4
  , BLOCK_WHEAT                   = 59 << 4
  , BLOCK_FARMLAND                = 60 << 4
  , BLOCK_FURNACE                 = 61 << 4
  , BLOCK_LIT_FURNACE             = 62 << 4
  , BLOCK_STANDING_SIGN           = 63 << 4
  , BLOCK_WOODEN_DOOR             = 64 << 4
  , BLOCK_LADDER                  = 65 << 4
  , BLOCK_RAIL                    = 66 << 4
  , BLOCK_STONE_STAIRS            = 67 << 4
  , BLOCK_WALL_SIGN               = 68 << 4
  , BLOCK_LEVER                   = 69 << 4
  , BLOCK_STONE_PRESSURE_PLATE    = 70 << 4
  , BLOCK_IRON_DOOR               = 71 << 4
  , BLOCK_WOODEN_PRESSURE_PLATE   = 72 << 4
  , BLOCK_REDSTONE_ORE            = 73 << 4
  , BLOCK_LIT_REDSTONE_ORE        = 74 << 4
  , BLOCK_UNLIT_REDSTONE_TORCH    = 75 << 4
  , BLOCK_REDSTONE_TORCH          = 76 << 4
  , BLOCK_STONE_BUTTON            = 77 << 4
  , BLOCK_SNOW_LAYER              = 78 << 4
  , BLOCK_ICE                     = 79 << 4
  , BLOCK_SNOW                    = 80 << 4
  , BLOCK_CACTUS                  = 81 << 4
  , BLOCK_CLAY                    = 82 << 4
  , BLOCK_REEDS                   = 83 << 4
  , BLOCK_JUKEBOX                 = 84 << 4
  , BLOCK_FENCE                   = 85 << 4
  , BLOCK_PUMPKIN                 = 86 << 4
  , BLOCK_NETHERRACK              = 87 << 4
  , BLOCK_SOUL_SAND               = 88 << 4
  , BLOCK_GLOWSTONE               = 89 << 4
  , BLOCK_PORTAL                  = 90 << 4
  , BLOCK_LIT_PUMPKIN             = 91 << 4
  , BLOCK_CAKE                    = 92 << 4
  , BLOCK_UNPOWERED_REPEATER      = 93 << 4
  , BLOCK_POWERED_REPEATER        = 94 << 4
  , BLOCK_STAINED_GLASS           = 95 << 4
  , BLOCK_TRAPDOOR                = 96 << 4
  , BLOCK_MONSTER_EGG             = 97 << 4
  , BLOCK_STONEBRICK              = 98 << 4
  , BLOCK_BROWN_MUSHROOM_BLOCK    = 99 << 4
  , BLOCK_RED_MUSHROOM_BLOCK      = 100 << 4
  , BLOCK_IRON_BARS               = 101 << 4
  , BLOCK_GLASS_PANE              = 102 << 4
  , BLOCK_MELON_BLOCK             = 103 << 4
  , BLOCK_PUMPKIN_STEM            = 104 << 4
  , BLOCK_MELON_STEM              = 105 << 4
  , BLOCK_VINE                    = 106 << 4
  , BLOCK_FENCE_GATE              = 107 << 4
  , BLOCK_BRICK_STAIRS            = 108 << 4
  , BLOCK_STONE_BRICK_STAIRS      = 109 << 4
  , BLOCK_MYCELIUM                = 110 << 4
  , BLOCK_WATERLILY               = 111 << 4
  , BLOCK_NETHER_BRICK            = 112 << 4
  , BLOCK_NETHER_BRICK_FENCE      = 113 << 4
  , BLOCK_NETHER_BRICK_STAIRS     = 114 << 4
  , BLOCK_NETHER_WART             = 115 << 4
  , BLOCK_ENCHANTING_TABLE        = 116 << 4
  , BLOCK_BREWING_STAND           = 117 << 4
  , BLOCK_CAULDRON                = 118 << 4
  , BLOCK_END_PORTAL              = 119 << 4
  , BLOCK_END_PORTAL_FRAME        = 120 << 4
  , BLOCK_END_STONE               = 121 << 4
  , BLOCK_DRAGON_EGG              = 122 << 4
  , BLOCK_REDSTONE_LAMP           = 123 << 4
  , BLOCK_LIT_REDSTONE_LAMP       = 124 << 4
  , BLOCK_DOUBLE_WOODEN_SLAB      = 125 << 4
  , BLOCK_WOODEN_SLAB             = 126 << 4
  , BLOCK_COCOA                   = 127 << 4
  , BLOCK_SANDSTONE_STAIRS        = 128 << 4
  , BLOCK_EMERALD_ORE             = 129 << 4
  , BLOCK_ENDER_CHEST             = 130 << 4
  , BLOCK_TRIPWIRE_HOOK           = 131 << 4
  , BLOCK_TRIPWIRE                = 132 << 4
  , BLOCK_EMERALD_BLOCK           = 133 << 4
  , BLOCK_SPRUCE_STAIRS           = 134 << 4
  , BLOCK_BIRCH_STAIRS            = 135 << 4
  , BLOCK_JUNGLE_STAIRS           = 136 << 4
  , BLOCK_COMMAND_BLOCK           = 137 << 4
  , BLOCK_BEACON                  = 138 << 4
  , BLOCK_COBBLESTONE_WALL        = 139 << 4
  , BLOCK_FLOWER_POT              = 140 << 4
  , BLOCK_CARROTS                 = 141 << 4
  , BLOCK_POTATOES                = 142 << 4
  , BLOCK_WOODEN_BUTTON           = 143 << 4
  , BLOCK_SKULL                   = 144 << 4
  , BLOCK_ANVIL                   = 145 << 4
  , BLOCK_TRAPPED_CHEST           = 146 << 4
  , BLOCK_LIGHT_WEIGHTED_PRESSURE_PLATE = 147 << 4
  , BLOCK_HEAVY_WEIGHTED_PRESSURE_PLATE = 148 << 4
  , BLOCK_UNPOWERED_COMPARATOR    = 149 << 4
  , BLOCK_POWERED_COMPARATOR      = 150 << 4
  , BLOCK_DAYLIGHT_DETECTOR       = 151 << 4
  , BLOCK_REDSTONE_BLOCK          = 152 << 4
  , BLOCK_QUARTZ_ORE              = 153 << 4
  , BLOCK_HOPPER                  = 154 << 4
  , BLOCK_QUARTZ_BLOCK            = 155 << 4
  , BLOCK_QUARTZ_STAIRS           = 156 << 4
  , BLOCK_ACTIVATOR_RAIL          = 157 << 4
  , BLOCK_DROPPER                 = 158 << 4
  , BLOCK_STAINED_HARDENED_CLAY   = 159 << 4
  , BLOCK_STAINED_GLASS_PANE      = 160 << 4
  , BLOCK_LEAVES2                 = 161 << 4
  , BLOCK_LOG2                    = 162 << 4
  , BLOCK_ACACIA_STAIRS           = 163 << 4
  , BLOCK_DARK_OAK_STAIRS         = 164 << 4
  , BLOCK_SLIME                   = 165 << 4
  , BLOCK_BARRIER                 = 166 << 4
  , BLOCK_IRON_TRAPDOOR           = 167 << 4
  , BLOCK_PRISMARINE              = 168 << 4
  , BLOCK_SEA_LANTERN             = 169 << 4
  , BLOCK_HAY_BLOCK               = 170 << 4
  , BLOCK_CARPET                  = 171 << 4
  , BLOCK_HARDENED_CLAY           = 172 << 4
  , BLOCK_COAL_BLOCK              = 173 << 4
  , BLOCK_PACKED_ICE              = 174 << 4
  , BLOCK_DOUBLE_PLANT            = 175 << 4
  , BLOCK_STANDING_BANNER         = 176 << 4
  , BLOCK_WALL_BANNER             = 177 << 4
  , BLOCK_DAYLIGHT_DETECTOR_INVERTED = 178 << 4
  , BLOCK_RED_SANDSTONE           = 179 << 4
  , BLOCK_RED_SANDSTONE_STAIRS    = 180 << 4
  , BLOCK_DOUBLE_STONE_SLAB2      = 181 << 4
  , BLOCK_STONE_SLAB2             = 182 << 4
  , BLOCK_SPRUCE_FENCE_GATE       = 183 << 4
  , BLOCK_BIRCH_FENCE_GATE        = 184 << 4
  , BLOCK_JUNGLE_FENCE_GATE       = 185 << 4
  , BLOCK_DARK_OAK_FENCE_GATE     = 186 << 4
  , BLOCK_ACACIA_FENCE_GATE       = 187 << 4
  , BLOCK_SPRUCE_FENCE            = 188 << 4
  , BLOCK_BIRCH_FENCE             = 189 << 4
  , BLOCK_JUNGLE_FENCE            = 190 << 4
  , BLOCK_DARK_OAK_FENCE          = 191 << 4
  , BLOCK_ACACIA_FENCE            = 192 << 4
  , BLOCK_SPRUCE_DOOR             = 193 << 4
  , BLOCK_BIRCH_DOOR              = 194 << 4
  , BLOCK_JUNGLE_DOOR             = 195 << 4
  , BLOCK_ACACIA_DOOR             = 196 << 4
  , BLOCK_DARK_OAK_DOOR           = 197 << 4
;

struct BlockVariant {
    std::string model;
    int x = 0;
    int y = 0;
    bool uvlock = false;
};

struct BlockDescriptor {
    std::string identifier;
    float hardness = 0.f;
    float resistance = 0.f;
    std::string stepSound = "";
    int lightOpacity = 0;
    int lightLevel = 0;
    uint32_t flags = 0;

    std::map<std::string, std::vector<BlockVariant>> variants; // propertydescriptor -> list of variants

    std::string string() {
        return identifier;
    }
};

struct BlockModel {
    std::string identifier;
    std::string comment;
    std::array<std::string, 6> textures; // down, up, north, south, west, east
};

constexpr uint16_t blockId(const Block b) {
    return b >> 4;
}

constexpr uint8_t blockMeta(const Block b) {
    return b & 0xF;
}

class BlockUtil {
public:
    static std::map<uint16_t, BlockDescriptor> blockData;
    static std::map<uint16_t, std::vector<BlockModel>> blockModels;

    static void loadBlockData(const std::string &filename) {
        std::string data = Zstd::readZstdFile(filename);
        std::stringstream ss(data);
        std::string line;
        std::getline(ss, line); // skip header
        while (std::getline(ss, line)) {
            if (line.empty()) continue;

            std::stringstream ss2(line);
            std::string idStr, subtypeStr, identifier, hardnessStr, resistanceStr, stepStr, opacityStr, lightStr, flagsStr;
            std::getline(ss2, idStr, ',');
            std::getline(ss2, subtypeStr, ',');
            std::getline(ss2, identifier, ',');
            std::getline(ss2, hardnessStr, ',');
            std::getline(ss2, resistanceStr, ',');
            std::getline(ss2, stepStr, ',');
            std::getline(ss2, opacityStr, ',');
            std::getline(ss2, lightStr, ',');
            std::getline(ss2, flagsStr, ',');

            uint16_t id = std::stoi(idStr);
            uint8_t subtype = std::stoi(subtypeStr);
            uint16_t key = (id << 4) | (subtype & 0xF);

            BlockDescriptor desc;
            desc.identifier = identifier;
            desc.hardness = std::stof(hardnessStr);
            desc.resistance = std::stof(resistanceStr);
            desc.stepSound = stepStr;
            desc.lightOpacity = std::stoi(opacityStr);
            desc.lightLevel = std::stoi(lightStr);
            desc.flags = std::stoul(flagsStr);

            blockData[key] = std::move(desc);
        }
    }
    static void loadBlockStates(const std::string &filename) {
        std::string data = Zstd::readZstdFile(filename);
        std::stringstream ss(data);
        std::string line;
        std::getline(ss, line); // skip header
        while (std::getline(ss, line)) {
            if (line.empty()) continue;

            std::stringstream ss2(line);
            std::string localName, propertyDescriptor, countStr;
            std::getline(ss2, localName, ',');
            std::getline(ss2, propertyDescriptor, ',');
            std::getline(ss2, countStr, ',');

            int count = std::stoi(countStr);
            std::vector<BlockVariant> variants;
            for (int i = 0; i < count; ++i) {
                std::string model, xStr, yStr, uvStr;
                std::getline(ss2, model, ',');
                std::getline(ss2, xStr, ',');
                std::getline(ss2, yStr, ',');
                std::getline(ss2, uvStr, ',');

                BlockVariant v;
                v.model = model;
                v.x = xStr.empty() ? 0 : std::stoi(xStr);
                v.y = yStr.empty() ? 0 : std::stoi(yStr);
                v.uvlock = uvStr == "true" || uvStr == "1";
                variants.push_back(v);
            }

            for (auto &desc: blockData | std::views::values) {
                if (desc.identifier == localName) {
                    desc.variants[propertyDescriptor] = std::move(variants);
                    break;
                }
            }
        }
    }
    static void loadBlockModels(const std::string &filename) {
        std::string data = Zstd::readZstdFile(filename);
        std::stringstream ss(data);
        std::string line;
        std::getline(ss, line); // skip header
        while (std::getline(ss, line)) {
            if(line.empty()) continue;

            std::stringstream ls(line);
            std::string identifier, comment, from, to;
            std::array<std::string, 6> textures;

            std::getline(ls, identifier, ',');
            std::getline(ls, comment, ',');
            std::getline(ls, from, ',');
            std::getline(ls, to, ',');

            for(int i = 0; i < 6; ++i) {
                std::string tex, uv;
                std::getline(ls, tex, ',');
                std::getline(ls, uv, ',');
                if(!tex.empty() && tex.rfind("blocks/", 0) == 0)
                    tex = tex.substr(7); // remove "blocks/"
                textures[i] = tex;
            }

            // assign to all IDs that match identifier
            for(auto &[key, desc] : blockData) {
                if(desc.identifier == identifier) {
                    if (!blockModels[key].empty()) {
                        std::wcerr << "[WARN] Block " << identifier.c_str() << " registered twice in blockmodels.dat" << "\n";
                        break;
                    }
                    blockModels[key].push_back({identifier, comment, textures});
                }
            }
        }
    }

    static const std::string& getBlockTexture(const Block block, const int face) {
        static const std::string fallback = "dirt";
        //
        // const int id = blockId(block);
        //
        // if (id == blockId(BLOCK_DIRT)) printf("A\n");
        //
        // const auto itDesc = blockData.find(block);
        // if (itDesc == blockData.end()) return fallback;
        //
        // if (id == blockId(BLOCK_DIRT)) printf("Desc: %s\n", itDesc->second.identifier.c_str());
        //
        // const auto& desc = itDesc->second;
        // if (desc.variants.empty()) return fallback;
        //
        // if (id == blockId(BLOCK_DIRT)) printf("Var size: %lu\n", desc.variants.begin()->second.size());
        //
        // const auto& firstVariantList = desc.variants.begin()->second;
        // if (firstVariantList.empty()) return fallback;
        //
        // const auto& variant = firstVariantList[0];
        // const auto& modelName = variant.model;
        //
        // if (id == blockId(BLOCK_DIRT)) printf("First var: %s\n", variant.model.c_str());

        const auto itModels = blockModels.find(block);
        if (itModels == blockModels.end()) return fallback;

        const auto& vec = itModels->second[0];

        printf("T: %d %d %s\n", blockId(block), face, vec.textures[face % vec.textures.size()].c_str());

        return vec.textures[face % vec.textures.size()];
    }

    static constexpr bool isOpaque(const Block block) {
        switch (blockId(block)) {
            // Transparent / non-opaque blocks
            case blockId(BLOCK_ICE):
            case blockId(BLOCK_PACKED_ICE):
            case blockId(BLOCK_GLASS):
            case blockId(BLOCK_STAINED_GLASS):
            case blockId(BLOCK_STAINED_GLASS_PANE):
            case blockId(BLOCK_LEAVES):
            case blockId(BLOCK_LEAVES2):
            case blockId(BLOCK_TALLGRASS):
            case blockId(BLOCK_DEADBUSH):
            case blockId(BLOCK_YELLOW_FLOWER):
            case blockId(BLOCK_RED_FLOWER):
            case blockId(BLOCK_WATER):
            case blockId(BLOCK_FLOWING_WATER):
            case blockId(BLOCK_LAVA):
            case blockId(BLOCK_FLOWING_LAVA):
            case blockId(BLOCK_VINE):
            case blockId(BLOCK_CARPET):
            case blockId(BLOCK_SNOW_LAYER):
            case blockId(BLOCK_SLIME):
            case blockId(BLOCK_FENCE):
            case blockId(BLOCK_FENCE_GATE):
            case blockId(BLOCK_TRAPDOOR):
            case blockId(BLOCK_LADDER):
            case blockId(BLOCK_STONE_BUTTON):
            case blockId(BLOCK_STONE_PRESSURE_PLATE):
            case blockId(BLOCK_WOODEN_PRESSURE_PLATE):
            case blockId(BLOCK_HEAVY_WEIGHTED_PRESSURE_PLATE):
            case blockId(BLOCK_LIGHT_WEIGHTED_PRESSURE_PLATE):
                return false;
            default:
                return true;
        }
    }

    static constexpr float getSlipperiness(const Block block) {
        switch (blockId(block)) {
            case blockId(BLOCK_ICE): case blockId(BLOCK_PACKED_ICE): return 0.98f;
            case blockId(BLOCK_SOUL_SAND): return 0.5f;
            case blockId(BLOCK_SLIME): return 0.8f;
            default: return 0.6f;
        }
    }
};

struct LocalBlock {
    const Block block;
    const int x, y, z;

    [[nodiscard]] double calculateXOffset(const AABB &other, double dx) const {
        if (!BlockUtil::isOpaque(block)) return dx;

        const double minX = x, minY = y, minZ = z,
                     maxX = x + 1, maxY = y + 1, maxZ = z + 1;

        if (other.maxY > minY && other.minY < maxY &&
            other.maxZ > minZ && other.minZ < maxZ) {

            if (dx > 0.0 && other.maxX <= minX)
                dx = std::min(dx, minX - other.maxX);
            if (dx < 0.0 && other.minX >= maxX)
                dx = std::max(dx, maxX - other.minX);
            }

        return dx;
    }

    [[nodiscard]] double calculateYOffset(const AABB &other, double dy) const {
        if (!BlockUtil::isOpaque(block)) return dy;

        const double minX = x, minY = y, minZ = z,
                     maxX = x + 1, maxY = y + 1, maxZ = z + 1;

        if (other.maxX > minX && other.minX < maxX &&
            other.maxZ > minZ && other.minZ < maxZ) {

            if (dy > 0.0 && other.maxY <= minY)
                dy = std::min(dy, minY - other.maxY);
            if (dy < 0.0 && other.minY >= maxY)
                dy = std::max(dy, maxY - other.minY);
            }

        return dy;
    }

    [[nodiscard]] double calculateZOffset(const AABB &other, double dz) const {
        if (!BlockUtil::isOpaque(block)) return dz;

        const double minX = x, minY = y, minZ = z,
                     maxX = x + 1, maxY = y + 1, maxZ = z + 1;

        if (other.maxX > minX && other.minX < maxX &&
            other.maxY > minY && other.minY < maxY) {

            if (dz > 0.0 && other.maxZ <= minZ)
                dz = std::min(dz, minZ - other.maxZ);
            if (dz < 0.0 && other.minZ >= maxZ)
                dz = std::max(dz, maxZ - other.minZ);
            }

        return dz;
    }
};