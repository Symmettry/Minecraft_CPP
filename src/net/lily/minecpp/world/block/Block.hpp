#pragma once
#ifndef BLOCK_HPP
#define BLOCK_HPP

#include <algorithm>
#include <string>
#include <glm/vec3.hpp>
#include "../../util/AABB.hpp"

// _Name = unimplemented
enum class Material {
    Air,
    Stone,
    Grass,
    Dirt,
    _Cobblestone,
    _WoodenPlanks,
    _OakSapling,
    _Bedrock,
    Water,
    _Water2,
    _Lava,
    _Lava2,
    _Sand,
    _Gravel,
    _GoldOre,
    _IronOre,
    _CoalOre,
    _Wood,
    _Leaves,
    _Sponge,
    _Glass,
    _LapisLazuliOre,
    _LapisLazuliBlock,
    _Dispenser,
    _Sandstone,
    _NoteBlock,
    _Bed,
    _PoweredRail,
    _DetectorRail,
    _StickyPiston,
    _Cobweb,
    _Grass,
    _DeadBush,
    _Piston,
    _PistonHead,
    _Wool,
    _PistonExtended,
    _Flower,
    _Flower2,
    _Mushroom,
    _Mushroom2,
    _BlockofGold,
    _BlockofIron,
    _StoneSlab,
    _StoneSlab2,
    _Bricks,
    _TNT,
    _Bookshelf,
    _MossStone,
    Obsidian,
    _Torch,
    _Fire,
    _MonsterSpawner,
    _OakWoodStairs,
    _Chest,
    _RedstoneDust,
    _DiamondOre,
    _BlockofDiamond,
    _CraftingTable,
    _Crops,
    _Farmland,
    _Furnace,
    _FurnaceOn,
    _Sign,
    _OakDoor,
    _Ladder,
    _Rail,
    _CobblestoneStairs,
    _SignWall,
    _Lever,
    _StonePressurePlate,
    _IronDoor,
    _WoodenPressurePlate,
    _RedstoneOre,
    _RedstoneOreLit,
    _RedstoneTorch,
    _RedstoneTorchOff,
    _Button,
    _SnowLayer,
    Ice,
    _SnowBlock,
    _Cactus,
    _Clay,
    _Sugarcane,
    _Jukebox,
    _OakFence,
    _Pumpkin,
    _Netherrack,
    _SoulSand,
    _Glowstone,
    _Portal,
    _JackoLantern,
    _Cake,
    _RedstoneRepeater,
    _RedstoneRepeater2,
    _StainedGlass,
    _WoodenTrapdoor,
    _StoneMonsterEgg,
    _StoneBricks,
    _Mushroom3,
    _Mushroom4,
    _IronBars,
    _GlassPane,
    _Melon,
    _PumpkinStem,
    _PumpkinStem2,
    _Vines,
    _OakFenceGate,
    _BrickStairs,
    _StoneBrickStairs,
    _Mycelium,
    _LilyPad,
    _NetherBrick,
    _NetherBrickFence,
    _NetherBrickStairs,
    _NetherWart,
    _EnchantmentTable,
    _BrewingStand,
    _Cauldron,
    _EndPortal,
    _EndPortal2,
    _EndStone,
    _DragonEgg,
    _RedstoneLamp,
    _RedstoneLamp2,
    _WoodSlab,
    _WoodSlab2,
    _Cocoa,
    _SandstoneStairs,
    _EmeraldOre,
    _EnderChest,
    _TripwireHook,
    _Tripwire,
    _BlockofEmerald,
    _SpruceWoodStairs,
    _BirchWoodStairs,
    _JungleWoodStairs,
    _CommandBlock,
    _Beacon,
    _CobblestoneWall,
    _FlowerPot,
    _Carrots,
    _Potatoes,
    _Button2,
    __skeleton_skull,
    _Anvil,
    _TrappedChest,
    _WeightedPressurePlate_Light,
    _WeightedPressurePlate_Heavy,
    _RedstoneComparator,
    _RedstoneComparato2r,
    _DaylightSensor,
    _BlockofRedstone,
    _NetherQuartzOre,
    _Hopper,
    _BlockofQuartz,
    _QuartzStairs,
    _ActivatorRail,
    _Dropper,
    _StainedClay,
    _StainedGlassPane,
    _Leaves2,
    _Wood2,
    _AcaciaWoodStairs,
    _DarkOakWoodStairs,
    _SlimeBlock,
    _Barrier,
    _IronTrapdoor,
    _Prismarine,
    _SeaLantern,
    _HayBale,
    _Carpet,
    _HardenedClay,
    _BlockofCoal,
    _PackedIce,
    _Plant,
    _WhiteBanner,
    _WhiteBanner2,
    _DaylightSensor2,
    _RedSandstone,
    _RedSandstoneStairs,
    _RedSandstoneSlab,
    _RedSandstoneSlab2,
    _SpruceFenceGate,
    _BirchFenceGate,
    _JungleFenceGate,
    _DarkOakFenceGate,
    _AcaciaFenceGate,
    _SpruceFence,
    _BirchFence,
    _JungleFence,
    _DarkOakFence,
    _AcaciaFence,
    _SpruceDoor,
    _BirchDoor,
    _JungleDoor,
    _AcaciaDoor,
    _DarkOakDoor,
};

using BlockPos = glm::vec<3, int>;

class Block {
public:
    virtual ~Block() = default;

    Material material;
    BlockPos position;

    Block() : material(Material::Air), position(0,0,0) {}
    Block(const int x, const int y, const int z) : material(Material::Air), position(x, y, z) {}
    explicit Block(const Material t, const int x, const int y, const int z) : material(t), position(x, y, z) {}

    [[nodiscard]] virtual bool isOpaque() const {
        return false;
    }

    [[nodiscard]] AABB getBoundingBox() const {
        return {minX(), minY(), minZ(), maxX(), maxY(), maxZ()};
    }

    [[nodiscard]] double minX() const { return position.x; }
    [[nodiscard]] double minY() const { return position.y; }
    [[nodiscard]] double minZ() const { return position.z; }
    [[nodiscard]] double maxX() const { return position.x + 1; }
    [[nodiscard]] double maxY() const { return position.y + 1; }
    [[nodiscard]] double maxZ() const { return position.z + 1; }

    [[nodiscard]] double calculateXOffset(const AABB &other, double dx) const;
    [[nodiscard]] double calculateYOffset(const AABB &other, double dy) const;
    [[nodiscard]] double calculateZOffset(const AABB &other, double dz) const;

    [[nodiscard]] virtual float slipperiness() const {
        return 0.6f;
    }

    [[nodiscard]] virtual std::string getName() const = 0;
    [[nodiscard]] virtual std::string getTextureName(int face) const = 0;

    virtual void onPlace() {}
    virtual void onBreak() {}

};

#endif
