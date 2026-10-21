#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace DebugNames {
inline std::string blockRegistryName(const uint16_t id) {
    if (id == 217) return "minecraft:structure_void";
    constexpr std::string_view names =
        "air stone grass dirt cobblestone planks sapling bedrock flowing_water water flowing_lava lava sand gravel gold_ore iron_ore coal_ore log leaves sponge glass lapis_ore lapis_block dispenser sandstone noteblock bed golden_rail detector_rail sticky_piston web tallgrass deadbush piston piston_head wool piston_extension yellow_flower red_flower brown_mushroom red_mushroom gold_block iron_block double_stone_slab stone_slab brick_block tnt bookshelf mossy_cobblestone obsidian torch fire mob_spawner oak_stairs chest redstone_wire diamond_ore diamond_block crafting_table wheat farmland furnace lit_furnace standing_sign wooden_door ladder rail stone_stairs wall_sign lever stone_pressure_plate iron_door wooden_pressure_plate redstone_ore lit_redstone_ore unlit_redstone_torch redstone_torch stone_button snow_layer ice snow cactus clay reeds jukebox fence pumpkin netherrack soul_sand glowstone portal lit_pumpkin cake unpowered_repeater powered_repeater stained_glass trapdoor monster_egg stonebrick brown_mushroom_block red_mushroom_block iron_bars glass_pane melon_block pumpkin_stem melon_stem vine fence_gate brick_stairs stone_brick_stairs mycelium waterlily nether_brick nether_brick_fence nether_brick_stairs nether_wart enchanting_table brewing_stand cauldron end_portal end_portal_frame end_stone dragon_egg redstone_lamp lit_redstone_lamp double_wooden_slab wooden_slab cocoa sandstone_stairs emerald_ore ender_chest tripwire_hook tripwire emerald_block spruce_stairs birch_stairs jungle_stairs command_block beacon cobblestone_wall flower_pot carrots potatoes wooden_button skull anvil trapped_chest light_weighted_pressure_plate heavy_weighted_pressure_plate unpowered_comparator powered_comparator daylight_detector redstone_block quartz_ore hopper quartz_block quartz_stairs activator_rail dropper stained_hardened_clay stained_glass_pane leaves2 log2 acacia_stairs dark_oak_stairs slime barrier iron_trapdoor prismarine sea_lantern hay_block carpet hardened_clay coal_block packed_ice double_plant standing_banner wall_banner daylight_detector_inverted red_sandstone red_sandstone_stairs double_stone_slab2 stone_slab2 spruce_fence_gate birch_fence_gate jungle_fence_gate dark_oak_fence_gate acacia_fence_gate spruce_fence birch_fence jungle_fence dark_oak_fence acacia_fence spruce_door birch_door jungle_door acacia_door dark_oak_door";
    size_t begin = 0;
    for (uint16_t current = 0; current < id; ++current) {
        begin = names.find(' ', begin);
        if (begin == std::string_view::npos) return "minecraft:unknown";
        ++begin;
    }
    const size_t end = names.find(' ', begin);
    if (begin >= names.size()) return "minecraft:unknown";
    return "minecraft:" + std::string(names.substr(begin, end - begin));
}

inline std::string biomeName(const uint8_t id) {
    switch (id) {
        case 0: return "Ocean"; case 1: return "Plains"; case 2: return "Desert";
        case 3: return "Extreme Hills"; case 4: return "Forest"; case 5: return "Taiga";
        case 6: return "Swampland"; case 7: return "River"; case 8: return "Hell";
        case 9: return "The End"; case 10: return "FrozenOcean"; case 11: return "FrozenRiver";
        case 12: return "Ice Plains"; case 13: return "Ice Mountains"; case 14: return "MushroomIsland";
        case 15: return "MushroomIslandShore"; case 16: return "Beach"; case 17: return "DesertHills";
        case 18: return "ForestHills"; case 19: return "TaigaHills"; case 20: return "Extreme Hills Edge";
        case 21: return "Jungle"; case 22: return "JungleHills"; case 23: return "JungleEdge";
        case 24: return "Deep Ocean"; case 25: return "Stone Beach"; case 26: return "Cold Beach";
        case 27: return "Birch Forest"; case 28: return "Birch Forest Hills"; case 29: return "Roofed Forest";
        case 30: return "Cold Taiga"; case 31: return "Cold Taiga Hills"; case 32: return "Mega Taiga";
        case 33: return "Mega Taiga Hills"; case 34: return "Extreme Hills+"; case 35: return "Savanna";
        case 36: return "Savanna Plateau"; case 37: return "Mesa"; case 38: return "Mesa Plateau F";
        case 39: return "Mesa Plateau";
        case 129: return "Sunflower Plains"; case 130: return "Desert M"; case 131: return "Extreme Hills M";
        case 132: return "Flower Forest"; case 133: return "Taiga M"; case 134: return "Swampland M";
        case 140: return "Ice Plains Spikes"; case 149: return "Jungle M"; case 151: return "JungleEdge M";
        case 155: return "Birch Forest M"; case 156: return "Birch Forest Hills M";
        case 157: return "Roofed Forest M"; case 158: return "Cold Taiga M"; case 160: return "Mega Spruce Taiga";
        case 161: return "Redwood Taiga Hills M"; case 162: return "Extreme Hills+ M";
        case 163: return "Savanna M"; case 164: return "Savanna Plateau M"; case 165: return "Mesa (Bryce)";
        case 166: return "Mesa Plateau F M"; case 167: return "Mesa Plateau M";
        default: return "Unknown (" + std::to_string(id) + ")";
    }
}
}
