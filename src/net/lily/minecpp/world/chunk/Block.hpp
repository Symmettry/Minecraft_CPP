#pragma once
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <array>
#include <iostream>
#include <ranges>
#include <string.h>
#include <unordered_map>

#include "net/lily/minecpp/render/BlockAtlas.hpp"
#include "net/lily/minecpp/util/AABB.hpp"
#include "net/lily/minecpp/util/Zstd.hpp"

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
  , BLOCK_STRUCTURE_VOID          = 217 << 4
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
    static constexpr size_t BLOCK_STATE_COUNT = 4096;
    static constexpr uint16_t INVALID_TEXTURE = 0xFFFF;
    static const char* textureIds[400];
    static const char* fallbackTexture;
    static uint16_t blockTextures[BLOCK_STATE_COUNT * 6];

    static constexpr const char* dyeColorNames[16] = {
        "white", "orange", "magenta", "light_blue", "yellow", "lime", "pink", "gray",
        "silver", "cyan", "purple", "blue", "brown", "green", "red", "black"
    };

    static const char* getAtlasTexturePtr(const BlockAtlas& atlas, const std::string& texture) {
        const auto it = std::ranges::find_if(atlas.blockAtlasPos
                                                     ,
                                                     [&](const auto &pair){ return strcmp(pair.first, texture.c_str()) == 0; }
                );
        if (it == nullptr || it == atlas.blockAtlasPos.end())
            throw std::runtime_error("Texture not existent in block atlas: " + texture);
        return it->first;
    }

    static int convertFacing(const int face) {
        switch (face) {
            case 0: return 5; // dOWN
            case 1: return 4; // uP
            case 2: return 0; // nORTH
            case 3: return 1; // sOUTH
            case 4: return 3; // wEST
            case 5: return 2; // eAST
            default: return -1; // invalid index
        }
    }

    static void loadBlockTextures(const BlockAtlas& atlas, const std::string &filename) {
        std::string data = Zstd::readZstdFile(filename);
        std::stringstream ss(data);
        std::string line;
        std::getline(ss, line); // skip header
        int t = 0;

        std::fill_n(blockTextures, BLOCK_STATE_COUNT * 6, INVALID_TEXTURE);

        std::unordered_map<std::string, int> textureMap{};

        const auto getTextureId = [&](const std::string& texture) {
            if (const auto it = textureMap.find(texture); it != textureMap.end()) return it->second;
            textureIds[t] = getAtlasTexturePtr(atlas, texture);
            textureMap[texture] = t;
            return t++;
        };

        while (std::getline(ss, line)) {
            if (line.empty()) continue;

            std::stringstream ss2(line);
            std::string sblockId, sface, texture;
            std::getline(ss2, sblockId, ',');
            std::getline(ss2, sface, ',');
            std::getline(ss2, texture, ',');

            if (texture == "missingno") continue; // ignore missing

            const int blockId = std::stoi(sblockId), face = convertFacing(std::stoi(sface));

            if (blockId < 0 || blockId >= BLOCK_STATE_COUNT || face < 0 || face >= 6) {
                throw std::runtime_error("Invalid block texture mapping: " + line);
            }

            blockTextures[blockId * 6 + face] = getTextureId(texture);
        }

        constexpr const char* leafTextures[] = {
            "leaves_oak", "leaves_spruce", "leaves_birch", "leaves_jungle"
        };
        for (int metadata = 0; metadata < 16; ++metadata) {
            const int texture = getTextureId(leafTextures[metadata & 3]);
            std::fill_n(blockTextures + ((BLOCK_LEAVES | metadata) * 6), 6, texture);
        }

        constexpr const char* leaf2Textures[] = {"leaves_acacia", "leaves_big_oak"};
        for (int metadata = 0; metadata < 16; ++metadata) {
            const int texture = getTextureId(leaf2Textures[metadata & 1]);
            std::fill_n(blockTextures + ((BLOCK_LEAVES2 | metadata) * 6), 6, texture);
        }

        constexpr const char* grassTextures[] = {"deadbush", "tallgrass", "fern"};
        for (int metadata = 0; metadata < 16; ++metadata) {
            const int variant = metadata == 1 ? 1 : metadata == 2 ? 2 : 0;
            const int texture = getTextureId(grassTextures[variant]);
            std::fill_n(blockTextures + ((BLOCK_TALLGRASS | metadata) * 6), 6, texture);
        }

        const int snowTexture = getTextureId("snow");
        for (int metadata = 0; metadata < 8; ++metadata) {
            std::fill_n(blockTextures + ((BLOCK_SNOW_LAYER | metadata) * 6), 6, snowTexture);
        }
        for (const Block slab : {BLOCK_STONE_SLAB, BLOCK_WOODEN_SLAB, BLOCK_STONE_SLAB2}) {
            for (int metadata = 0; metadata < 16; ++metadata) {
                uint16_t* faces = blockTextures + ((slab | metadata) * 6);
                if (faces[4] == INVALID_TEXTURE && faces[5] != INVALID_TEXTURE) faces[4] = faces[5];
                if (faces[5] == INVALID_TEXTURE && faces[4] != INVALID_TEXTURE) faces[5] = faces[4];
            }
        }
        fallbackTexture = getAtlasTexturePtr(atlas, "dirt");

        printf("[BlockRegistrar] Registered %d block values, consumed %zu bytes memory.\n", t, sizeof(textureIds) + sizeof(blockTextures));
    }

    // cross-shaped plants whose dropped-item icon is a flat texture, not a cube; nullptr if not one of these
    static const char* getCrossPlantItemTexture(const Block block) {
        switch (blockId(block)) {
            case blockId(BLOCK_SAPLING): {
                static constexpr const char* saplingTextures[] = {
                    "sapling_oak", "sapling_spruce", "sapling_birch",
                    "sapling_jungle", "sapling_acacia", "sapling_roofed_oak"
                };
                return saplingTextures[std::min<uint8_t>(blockMeta(block) & 7, 5)];
            }
            case blockId(BLOCK_TALLGRASS): {
                const uint8_t meta = blockMeta(block);
                return meta == 1 ? "tallgrass" : meta == 2 ? "fern" : "deadbush";
            }
            case blockId(BLOCK_DEADBUSH):
                return "deadbush";
            case blockId(BLOCK_YELLOW_FLOWER):
                return "flower_dandelion";
            case blockId(BLOCK_RED_FLOWER): {
                static constexpr const char* redFlowerTextures[] = {
                    "flower_rose", "flower_blue_orchid", "flower_allium", "flower_houstonia",
                    "flower_tulip_red", "flower_tulip_orange", "flower_tulip_white",
                    "flower_tulip_pink", "flower_oxeye_daisy"
                };
                return redFlowerTextures[std::min<uint8_t>(blockMeta(block), 8)];
            }
            case blockId(BLOCK_RAIL): return "rail_normal";
            case blockId(BLOCK_GOLDEN_RAIL): return "rail_golden";
            case blockId(BLOCK_DETECTOR_RAIL): return "rail_detector";
            case blockId(BLOCK_ACTIVATOR_RAIL): return "rail_activator";
            case blockId(BLOCK_TORCH): return "torch_on";
            case blockId(BLOCK_REDSTONE_TORCH): return "redstone_torch_on";
            case blockId(BLOCK_LADDER): return "ladder";
            case blockId(BLOCK_WATERLILY): return "waterlily";
            case blockId(BLOCK_WEB): return "web";
            case blockId(BLOCK_BROWN_MUSHROOM):
                return "mushroom_brown";
            case blockId(BLOCK_RED_MUSHROOM):
                return "mushroom_red";
            default:
                return nullptr;
        }
    }

    static const char* getBlockTexture(const Block block, const int face) {
        if (block >= BLOCK_STATE_COUNT || face < 0 || face >= 6) return fallbackTexture;
        const uint16_t tex = blockTextures[block * 6 + face];
        if (tex != INVALID_TEXTURE) return textureIds[tex];
        for (int fallbackFace = 0; fallbackFace < 6; ++fallbackFace) {
            const uint16_t fallback = blockTextures[block * 6 + fallbackFace];
            if (fallback != INVALID_TEXTURE) return textureIds[fallback];
        }
        return fallbackTexture;
    }

    static constexpr bool isOpaque(const Block block) {
        switch (blockId(block)) {
            // transparent / non-opaque blocks
            case blockId(BLOCK_AIR):
            case blockId(BLOCK_BARRIER):
            case blockId(BLOCK_ICE):
            case blockId(BLOCK_PACKED_ICE):
            case blockId(BLOCK_GLASS):
            case blockId(BLOCK_STAINED_GLASS):
            case blockId(BLOCK_IRON_BARS):
            case blockId(BLOCK_GLASS_PANE):
            case blockId(BLOCK_STAINED_GLASS_PANE):
            case blockId(BLOCK_LEAVES):
            case blockId(BLOCK_LEAVES2):
            case blockId(BLOCK_TALLGRASS):
            case blockId(BLOCK_DOUBLE_PLANT):
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
            case blockId(BLOCK_WOODEN_BUTTON):
            case blockId(BLOCK_STANDING_SIGN):
            case blockId(BLOCK_WALL_SIGN):
            case blockId(BLOCK_STONE_PRESSURE_PLATE):
            case blockId(BLOCK_WOODEN_PRESSURE_PLATE):
            case blockId(BLOCK_HEAVY_WEIGHTED_PRESSURE_PLATE):
            case blockId(BLOCK_LIGHT_WEIGHTED_PRESSURE_PLATE):
            case blockId(BLOCK_STRUCTURE_VOID):
            case blockId(BLOCK_PISTON):
            case blockId(BLOCK_STICKY_PISTON):
            case blockId(BLOCK_PISTON_HEAD):
            // non-cube blocks that the mesher draws as plants, crops, rails, torches and ladders
            case blockId(BLOCK_SAPLING): case blockId(BLOCK_WEB):
            case blockId(BLOCK_BROWN_MUSHROOM): case blockId(BLOCK_RED_MUSHROOM):
            case blockId(BLOCK_TORCH): case blockId(BLOCK_REDSTONE_TORCH): case blockId(BLOCK_UNLIT_REDSTONE_TORCH):
            case blockId(BLOCK_RAIL): case blockId(BLOCK_GOLDEN_RAIL):
            case blockId(BLOCK_DETECTOR_RAIL): case blockId(BLOCK_ACTIVATOR_RAIL):
            case blockId(BLOCK_WHEAT): case blockId(BLOCK_CARROTS): case blockId(BLOCK_POTATOES):
            case blockId(BLOCK_NETHER_WART): case blockId(BLOCK_REEDS): case blockId(BLOCK_WATERLILY):
            case blockId(BLOCK_PUMPKIN_STEM): case blockId(BLOCK_MELON_STEM):
            case blockId(BLOCK_REDSTONE_WIRE): case blockId(BLOCK_LEVER):
            case blockId(BLOCK_TRIPWIRE_HOOK): case blockId(BLOCK_TRIPWIRE):
                return false;
            default:
                return !isStair(block) && isFullBlockShape(block);
        }
    }

    static constexpr bool isFullBlockShape(const Block block) {
        switch (blockId(block)) {
            case blockId(BLOCK_STONE_SLAB): case blockId(BLOCK_WOODEN_SLAB):
            case blockId(BLOCK_STONE_SLAB2): case blockId(BLOCK_WOODEN_DOOR):
            case blockId(BLOCK_IRON_DOOR): case blockId(BLOCK_SPRUCE_DOOR):
            case blockId(BLOCK_BIRCH_DOOR): case blockId(BLOCK_JUNGLE_DOOR):
            case blockId(BLOCK_ACACIA_DOOR): case blockId(BLOCK_DARK_OAK_DOOR):
            case blockId(BLOCK_FENCE): case blockId(BLOCK_NETHER_BRICK_FENCE):
            case blockId(BLOCK_SPRUCE_FENCE): case blockId(BLOCK_BIRCH_FENCE):
            case blockId(BLOCK_JUNGLE_FENCE): case blockId(BLOCK_DARK_OAK_FENCE):
            case blockId(BLOCK_ACACIA_FENCE): case blockId(BLOCK_FENCE_GATE):
            case blockId(BLOCK_SPRUCE_FENCE_GATE): case blockId(BLOCK_BIRCH_FENCE_GATE):
            case blockId(BLOCK_JUNGLE_FENCE_GATE): case blockId(BLOCK_DARK_OAK_FENCE_GATE):
            case blockId(BLOCK_ACACIA_FENCE_GATE): case blockId(BLOCK_FARMLAND):
            case blockId(BLOCK_SOUL_SAND): case blockId(BLOCK_CACTUS):
            case blockId(BLOCK_CAKE): case blockId(BLOCK_BED):
            case blockId(BLOCK_ENCHANTING_TABLE): case blockId(BLOCK_CHEST):
            case blockId(BLOCK_TRAPPED_CHEST): case blockId(BLOCK_ENDER_CHEST):
            case blockId(BLOCK_TRAPDOOR): case blockId(BLOCK_IRON_TRAPDOOR):
            case blockId(BLOCK_COBBLESTONE_WALL): case blockId(BLOCK_CAULDRON):
            case blockId(BLOCK_FLOWER_POT): case blockId(BLOCK_SKULL):
            case blockId(BLOCK_END_PORTAL_FRAME): case blockId(BLOCK_DAYLIGHT_DETECTOR):
            case blockId(BLOCK_DAYLIGHT_DETECTOR_INVERTED): case blockId(BLOCK_ANVIL):
            case blockId(BLOCK_UNPOWERED_REPEATER): case blockId(BLOCK_POWERED_REPEATER):
            case blockId(BLOCK_UNPOWERED_COMPARATOR): case blockId(BLOCK_POWERED_COMPARATOR):
            case blockId(BLOCK_STRUCTURE_VOID):
            case blockId(BLOCK_PISTON): case blockId(BLOCK_STICKY_PISTON):
            case blockId(BLOCK_PISTON_HEAD):
                return false;
            default: return true;
        }
    }

    // the block state used to draw a block item. Furnaces and dispensers have no meta 0 model; the item model shows them facing north, which is where their front texture is
    static Block itemBlockState(const int itemId, const int damage) {
        const Block base = static_cast<Block>(static_cast<uint16_t>(itemId) << 4);
        switch (itemId) {
            case blockId(BLOCK_FURNACE): case blockId(BLOCK_DISPENSER): case blockId(BLOCK_DROPPER):
                return static_cast<Block>(base | 2);
            default:
                return static_cast<Block>(base | (damage & 15));
        }
    }


    // Blocks drawn by the block-entity pass (Renderer::renderBlockEntityModels) instead of, or on top of, chunk geometry.
    // The mesher lists them per chunk so the renderer never has to scan blocks.
    static constexpr bool isBlockEntityModel(const Block block) {
        switch (blockId(block)) {
            case blockId(BLOCK_CHEST): case blockId(BLOCK_TRAPPED_CHEST): case blockId(BLOCK_ENDER_CHEST):
                return true; // the whole block is the model, so the chunk mesh skips it
            default: return false;
        }
    }
    static constexpr bool hasBlockEntityExtras(const Block block) {
        return blockId(block) == blockId(BLOCK_ENCHANTING_TABLE); // a table in the mesh plus a floating book
    }

    // Block.onBlockActivated has an effect: right clicking these interacts instead of placing a block against them
    static constexpr bool isActivatable(const Block block) {
        switch (blockId(block)) {
            case blockId(BLOCK_LEVER): case blockId(BLOCK_STONE_BUTTON): case blockId(BLOCK_WOODEN_BUTTON):
            case blockId(BLOCK_WOODEN_DOOR): case blockId(BLOCK_SPRUCE_DOOR): case blockId(BLOCK_BIRCH_DOOR):
            case blockId(BLOCK_JUNGLE_DOOR): case blockId(BLOCK_ACACIA_DOOR): case blockId(BLOCK_DARK_OAK_DOOR):
            case blockId(BLOCK_TRAPDOOR): case blockId(BLOCK_FENCE_GATE): case blockId(BLOCK_SPRUCE_FENCE_GATE):
            case blockId(BLOCK_BIRCH_FENCE_GATE): case blockId(BLOCK_JUNGLE_FENCE_GATE):
            case blockId(BLOCK_DARK_OAK_FENCE_GATE): case blockId(BLOCK_ACACIA_FENCE_GATE):
            case blockId(BLOCK_UNPOWERED_REPEATER): case blockId(BLOCK_POWERED_REPEATER):
            case blockId(BLOCK_UNPOWERED_COMPARATOR): case blockId(BLOCK_POWERED_COMPARATOR):
            case blockId(BLOCK_CHEST): case blockId(BLOCK_TRAPPED_CHEST): case blockId(BLOCK_ENDER_CHEST):
            case blockId(BLOCK_FURNACE): case blockId(BLOCK_LIT_FURNACE): case blockId(BLOCK_CRAFTING_TABLE):
            case blockId(BLOCK_DISPENSER): case blockId(BLOCK_DROPPER): case blockId(BLOCK_HOPPER):
            case blockId(BLOCK_NOTEBLOCK): case blockId(BLOCK_JUKEBOX): case blockId(BLOCK_BED):
            case blockId(BLOCK_ANVIL): case blockId(BLOCK_ENCHANTING_TABLE): case blockId(BLOCK_BREWING_STAND):
            case blockId(BLOCK_CAKE): case blockId(BLOCK_CAULDRON): case blockId(BLOCK_BEACON):
            case blockId(BLOCK_DAYLIGHT_DETECTOR): case blockId(BLOCK_DAYLIGHT_DETECTOR_INVERTED):
            case blockId(BLOCK_FLOWER_POT): case blockId(BLOCK_COMMAND_BLOCK): case blockId(BLOCK_TRIPWIRE_HOOK):
            case blockId(BLOCK_STANDING_SIGN): case blockId(BLOCK_WALL_SIGN): case blockId(BLOCK_DRAGON_EGG):
                return true;
            default: return false;
        }
    }
    // block.getLightValue
    static constexpr uint8_t getLightValue(const Block block) {
        switch (blockId(block)) {
            case blockId(BLOCK_FIRE): case blockId(BLOCK_LAVA): case blockId(BLOCK_FLOWING_LAVA):
            case blockId(BLOCK_GLOWSTONE): case blockId(BLOCK_LIT_REDSTONE_LAMP): case blockId(BLOCK_LIT_PUMPKIN):
            case blockId(BLOCK_SEA_LANTERN): case blockId(BLOCK_BEACON): case blockId(BLOCK_END_PORTAL):
                return 15;
            case blockId(BLOCK_TORCH): return 14;
            case blockId(BLOCK_LIT_FURNACE): return 13;
            case blockId(BLOCK_PORTAL): return 11;
            case blockId(BLOCK_LIT_REDSTONE_ORE): return 9;
            case blockId(BLOCK_REDSTONE_TORCH): case blockId(BLOCK_ENDER_CHEST): return 7;
            case blockId(BLOCK_BROWN_MUSHROOM): case blockId(BLOCK_BREWING_STAND):
            case blockId(BLOCK_DRAGON_EGG): case blockId(BLOCK_END_PORTAL_FRAME):
                return 1;
            default: return 0;
        }
    }

    static constexpr uint8_t getSkyLightOpacity(const Block block) {
        switch (blockId(block)) {
            case blockId(BLOCK_LEAVES):
            case blockId(BLOCK_LEAVES2):
                return 1;
            default:
                return isOpaque(block) ? 15 : 0;
        }
    }

    static constexpr bool isClimbable(const Block block) {
        switch (blockId(block)) {
            case blockId(BLOCK_VINE):
            case blockId(BLOCK_LADDER):
                return true;
            default:
                return false;
        }
    }

    static constexpr bool isStair(const Block block) {
        switch (blockId(block)) {
            case blockId(BLOCK_OAK_STAIRS): case blockId(BLOCK_STONE_STAIRS):
            case blockId(BLOCK_BRICK_STAIRS): case blockId(BLOCK_STONE_BRICK_STAIRS):
            case blockId(BLOCK_NETHER_BRICK_STAIRS): case blockId(BLOCK_SANDSTONE_STAIRS):
            case blockId(BLOCK_SPRUCE_STAIRS): case blockId(BLOCK_BIRCH_STAIRS):
            case blockId(BLOCK_JUNGLE_STAIRS): case blockId(BLOCK_QUARTZ_STAIRS):
            case blockId(BLOCK_ACACIA_STAIRS): case blockId(BLOCK_DARK_OAK_STAIRS):
            case blockId(BLOCK_RED_SANDSTONE_STAIRS): return true;
            default: return false;
        }
    }

    // blockStairs shapes (straight, inner and outer corners): the upper half of a stair as a mask of horizontal quadrants, bit = (z >= 8) * 2 + (x >= 8). `at(dx, dz)` returns the block beside it
    template <typename Getter>
    static uint8_t stairUpperQuadrants(const Block block, Getter&& at) {
        static constexpr int directions[4][2]{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}; // east, west, south, north
        const auto onSide = [](const int quadrant, const int dx, const int dz) {
            const int qx = quadrant & 1, qz = quadrant >> 1;
            return (dx > 0 && qx == 1) || (dx < 0 && qx == 0) || (dz > 0 && qz == 1) || (dz < 0 && qz == 0);
        };
        const uint8_t meta = blockMeta(block);
        const int fx = directions[meta & 3][0], fz = directions[meta & 3][1];
        const auto sameHalf = [&](const Block other) { return isStair(other) && (blockMeta(other) & 4) == (meta & 4); };
        const auto differentStairs = [&](const int dx, const int dz) {
            const Block other = at(dx, dz);
            return !isStair(other) || (blockMeta(other) & 7) != (meta & 7);
        };
        uint8_t mask = 0;
        const Block behind = at(fx, fz);
        if (sameHalf(behind)) {
            const int gx = directions[blockMeta(behind) & 3][0], gz = directions[blockMeta(behind) & 3][1];
            if (gx * fx + gz * fz == 0 && differentStairs(-gx, -gz)) { // outer corner: one quadrant
                for (int quadrant = 0; quadrant < 4; ++quadrant)
                    if (onSide(quadrant, fx, fz) && onSide(quadrant, gx, gz)) mask |= 1U << quadrant;
                return mask;
            }
        }
        for (int quadrant = 0; quadrant < 4; ++quadrant)
            if (onSide(quadrant, fx, fz)) mask |= 1U << quadrant;
        const Block front = at(-fx, -fz);
        if (sameHalf(front)) {
            const int gx = directions[blockMeta(front) & 3][0], gz = directions[blockMeta(front) & 3][1];
            if (gx * fx + gz * fz == 0 && differentStairs(gx, gz)) { // inner corner: an extra quadrant
                for (int quadrant = 0; quadrant < 4; ++quadrant)
                    if (onSide(quadrant, -fx, -fz) && onSide(quadrant, gx, gz)) mask |= 1U << quadrant;
            }
        }
        return mask;
    }

    static constexpr bool isFence(const Block block) {
        switch (blockId(block)) {
            case blockId(BLOCK_FENCE): case blockId(BLOCK_NETHER_BRICK_FENCE):
            case blockId(BLOCK_SPRUCE_FENCE): case blockId(BLOCK_BIRCH_FENCE):
            case blockId(BLOCK_JUNGLE_FENCE): case blockId(BLOCK_DARK_OAK_FENCE):
            case blockId(BLOCK_ACACIA_FENCE): return true;
            default: return false;
        }
    }

    static constexpr bool isFenceGate(const Block block) {
        switch (blockId(block)) {
            case blockId(BLOCK_FENCE_GATE): case blockId(BLOCK_SPRUCE_FENCE_GATE):
            case blockId(BLOCK_BIRCH_FENCE_GATE): case blockId(BLOCK_JUNGLE_FENCE_GATE):
            case blockId(BLOCK_DARK_OAK_FENCE_GATE): case blockId(BLOCK_ACACIA_FENCE_GATE): return true;
            default: return false;
        }
    }

    static constexpr bool isPane(const Block block) {
        switch (blockId(block)) {
            case blockId(BLOCK_IRON_BARS): case blockId(BLOCK_GLASS_PANE):
            case blockId(BLOCK_STAINED_GLASS_PANE): return true;
            default: return false;
        }
    }

    static constexpr bool isFullBlock(const Block block) {
        return isOpaque(block) && isFullBlockShape(block);
    }

    static constexpr bool occludesAmbientLight(const Block block) {
        switch (blockId(block)) {
            case blockId(BLOCK_LEAVES): case blockId(BLOCK_LEAVES2): return true;
            default: return isFullBlock(block);
        }
    }

    static constexpr bool canPaneConnectTo(const Block block) {
        switch (blockId(block)) {
            case blockId(BLOCK_GLASS): case blockId(BLOCK_STAINED_GLASS): return true;
            default: return isPane(block) || isFullBlock(block);
        }
    }

    static constexpr bool hasFullCubeCollision(const Block block) {
        if (isOpaque(block)) return true;

        switch (blockId(block)) {
            case blockId(BLOCK_ICE):
            case blockId(BLOCK_PACKED_ICE):
            case blockId(BLOCK_GLASS):
            case blockId(BLOCK_STAINED_GLASS):
            case blockId(BLOCK_LEAVES):
            case blockId(BLOCK_LEAVES2):
            case blockId(BLOCK_SLIME):
            case blockId(BLOCK_PISTON):
            case blockId(BLOCK_STICKY_PISTON):
                return true;
            default:
                return false;
        }
    }

    static constexpr bool isSolidMaterial(const Block block) {
        switch (blockId(block)) {
            case blockId(BLOCK_AIR):
            case blockId(BLOCK_FLOWING_WATER): case blockId(BLOCK_WATER):
            case blockId(BLOCK_FLOWING_LAVA): case blockId(BLOCK_LAVA):
            case blockId(BLOCK_SAPLING): case blockId(BLOCK_TALLGRASS):
            case blockId(BLOCK_DEADBUSH): case blockId(BLOCK_YELLOW_FLOWER):
            case blockId(BLOCK_RED_FLOWER): case blockId(BLOCK_BROWN_MUSHROOM):
            case blockId(BLOCK_RED_MUSHROOM): case blockId(BLOCK_TORCH):
            case blockId(BLOCK_FIRE): case blockId(BLOCK_REDSTONE_WIRE):
            case blockId(BLOCK_WHEAT): case blockId(BLOCK_STANDING_SIGN):
            case blockId(BLOCK_LADDER): case blockId(BLOCK_RAIL):
            case blockId(BLOCK_WALL_SIGN): case blockId(BLOCK_LEVER):
            case blockId(BLOCK_STONE_PRESSURE_PLATE):
            case blockId(BLOCK_WOODEN_PRESSURE_PLATE):
            case blockId(BLOCK_UNLIT_REDSTONE_TORCH): case blockId(BLOCK_REDSTONE_TORCH):
            case blockId(BLOCK_STONE_BUTTON): case blockId(BLOCK_SNOW_LAYER):
            case blockId(BLOCK_REEDS): case blockId(BLOCK_PORTAL):
            case blockId(BLOCK_UNPOWERED_REPEATER): case blockId(BLOCK_POWERED_REPEATER):
            case blockId(BLOCK_PUMPKIN_STEM): case blockId(BLOCK_MELON_STEM):
            case blockId(BLOCK_VINE): case blockId(BLOCK_WATERLILY):
            case blockId(BLOCK_NETHER_WART): case blockId(BLOCK_END_PORTAL):
            case blockId(BLOCK_COCOA): case blockId(BLOCK_TRIPWIRE_HOOK):
            case blockId(BLOCK_TRIPWIRE): case blockId(BLOCK_FLOWER_POT):
            case blockId(BLOCK_CARROTS): case blockId(BLOCK_POTATOES):
            case blockId(BLOCK_WOODEN_BUTTON):
            case blockId(BLOCK_LIGHT_WEIGHTED_PRESSURE_PLATE):
            case blockId(BLOCK_HEAVY_WEIGHTED_PRESSURE_PLATE):
            case blockId(BLOCK_UNPOWERED_COMPARATOR): case blockId(BLOCK_POWERED_COMPARATOR):
            case blockId(BLOCK_ACTIVATOR_RAIL): case blockId(BLOCK_CARPET):
            case blockId(BLOCK_DOUBLE_PLANT): case blockId(BLOCK_STANDING_BANNER):
            case blockId(BLOCK_WALL_BANNER):
            case blockId(BLOCK_GOLDEN_RAIL): case blockId(BLOCK_DETECTOR_RAIL):
            case blockId(BLOCK_STRUCTURE_VOID):
                return false;
            default:
                return true;
        }
    }

    static constexpr bool blocksMovement(const Block block) {
        switch (blockId(block)) {
            case blockId(BLOCK_WEB):
                return false;
            default:
                return isSolidMaterial(block);
        }
    }

    static constexpr uint8_t doorPlane(const uint8_t lowerMeta, const uint8_t upperMeta) {
        const uint8_t direction = lowerMeta & 3;
        if ((lowerMeta & 4) == 0) return static_cast<uint8_t>((direction + 3) & 3);
        const bool hingeRight = (upperMeta & 1) != 0;
        return static_cast<uint8_t>((direction + (hingeRight ? 2 : 0)) & 3);
    }

    static constexpr float getSlipperiness(const Block block) {
        switch (blockId(block)) {
            case blockId(BLOCK_ICE): case blockId(BLOCK_PACKED_ICE): return 0.98f;
            case blockId(BLOCK_SLIME): return 0.8f;
            default: return 0.6f;
        }
    }
};

struct LocalBlock {
    const Block block;
    const int x, y, z;
    const AABB bounds;

    LocalBlock(const Block block, const int x, const int y, const int z)
        : block(block), x(x), y(y), z(z), bounds(x, y, z, x + 1, y + 1, z + 1) {}

    LocalBlock(const Block block, const int x, const int y, const int z, const AABB& localBounds)
        : block(block), x(x), y(y), z(z),
          bounds(localBounds.offset(x, y, z)) {}

    [[nodiscard]] double calculateXOffset(const AABB &other, double dx) const {
        const double minX = bounds.minX, minY = bounds.minY, minZ = bounds.minZ,
                     maxX = bounds.maxX, maxY = bounds.maxY, maxZ = bounds.maxZ;

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
        const double minX = bounds.minX, minY = bounds.minY, minZ = bounds.minZ,
                     maxX = bounds.maxX, maxY = bounds.maxY, maxZ = bounds.maxZ;

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
        const double minX = bounds.minX, minY = bounds.minY, minZ = bounds.minZ,
                     maxX = bounds.maxX, maxY = bounds.maxY, maxZ = bounds.maxZ;

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
