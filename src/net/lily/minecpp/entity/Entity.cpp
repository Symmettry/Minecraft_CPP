#include "Entity.hpp"
#include "EntityMob.hpp"
#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/util/Math.hpp"
#include <cmath>
#include <optional>

namespace {
bool isWaterBlock(const Block block) {
    switch (blockId(block)) {
        case blockId(BLOCK_WATER):
        case blockId(BLOCK_FLOWING_WATER): return true;
        default: return false;
    }
}

bool isLavaBlock(const Block block) {
    switch (blockId(block)) {
        case blockId(BLOCK_LAVA):
        case blockId(BLOCK_FLOWING_LAVA): return true;
        default: return false;
    }
}

bool containsMaterial(const World& world, const AABB& bounds, const bool water) {
    const int minX = static_cast<int>(std::floor(bounds.minX));
    const int maxX = static_cast<int>(std::floor(bounds.maxX + 1.0));
    const int minY = static_cast<int>(std::floor(bounds.minY));
    const int maxY = static_cast<int>(std::floor(bounds.maxY + 1.0));
    const int minZ = static_cast<int>(std::floor(bounds.minZ));
    const int maxZ = static_cast<int>(std::floor(bounds.maxZ + 1.0));
    for (int x = minX; x < maxX; ++x) {
        for (int y = minY; y < maxY; ++y) {
            for (int z = minZ; z < maxZ; ++z) {
                const Block block = world.getBlockAt(x, y, z);
                if (water ? isWaterBlock(block) : isLavaBlock(block)) return true;
            }
        }
    }
    return false;
}

int effectiveFlowDecay(const World& world, const int x, const int y, const int z) {
    const Block block = world.getBlockAt(x, y, z);
    if (!isWaterBlock(block)) return -1;
    const int level = blockMeta(block);
    return level >= 8 ? 0 : level;
}

glm::dvec3 waterFlow(const World& world, const int x, const int y, const int z) {
    glm::dvec3 flow{};
    const int decay = effectiveFlowDecay(world, x, y, z);
    static constexpr int directions[4][2]{{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
    for (const auto& direction : directions) {
        const int neighborX = x + direction[0];
        const int neighborZ = z + direction[1];
        int neighborDecay = effectiveFlowDecay(world, neighborX, y, neighborZ);
        int difference = 0;
        if (neighborDecay < 0) {
            if (BlockUtil::blocksMovement(world.getBlockAt(neighborX, y, neighborZ))) continue;
            neighborDecay = effectiveFlowDecay(world, neighborX, y - 1, neighborZ);
            if (neighborDecay < 0) continue;
            difference = neighborDecay - (decay - 8);
        } else {
            difference = neighborDecay - decay;
        }
        flow.x += direction[0] * difference;
        flow.z += direction[1] * difference;
    }

    if (blockMeta(world.getBlockAt(x, y, z)) >= 8) {
        for (const auto& direction : directions) {
            const int neighborX = x + direction[0];
            const int neighborZ = z + direction[1];
            if (BlockUtil::blocksMovement(world.getBlockAt(neighborX, y, neighborZ))
                || BlockUtil::blocksMovement(world.getBlockAt(neighborX, y + 1, neighborZ))) {
                if (glm::length(flow) > 0.0) flow = glm::normalize(flow);
                flow.y -= 6.0;
                break;
            }
        }
    }
    return glm::length(flow) > 0.0 ? glm::normalize(flow) : flow;
}

bool accelerateInWater(const World& world, const AABB& bounds, Entity& entity) {
    const int minX = static_cast<int>(std::floor(bounds.minX));
    const int maxX = static_cast<int>(std::floor(bounds.maxX + 1.0));
    const int minY = static_cast<int>(std::floor(bounds.minY));
    const int maxY = static_cast<int>(std::floor(bounds.maxY + 1.0));
    const int minZ = static_cast<int>(std::floor(bounds.minZ));
    const int maxZ = static_cast<int>(std::floor(bounds.maxZ + 1.0));
    glm::dvec3 acceleration{};
    bool foundWater = false;
    for (int x = minX; x < maxX; ++x) {
        for (int y = minY; y < maxY; ++y) {
            for (int z = minZ; z < maxZ; ++z) {
                if (!world.isChunkAtLoaded(x, z)) return false;
                if (!isWaterBlock(world.getBlockAt(x, y, z))) continue;
                foundWater = true;
                acceleration += waterFlow(world, x, y, z);
            }
        }
    }
    if (entity.isPushedByWater() && glm::length(acceleration) > 0.0) {
        entity.velocity += glm::normalize(acceleration) * 0.014;
    }
    return foundWater;
}

struct RayHit {
    glm::dvec3 position{};
    double distance = 0.0;
    glm::ivec3 blockPosition{};
    std::array<AABB, 5> blockBounds{};
    uint8_t blockBoundsCount = 0;
    uint8_t sideHit = 0;
};

glm::dvec3 lookVector(const Entity& entity, const float partialTicks) {
    const float yaw = std::lerp(entity.lastRot.yaw, entity.rotation.yaw, partialTicks);
    const float pitch = std::lerp(entity.lastRot.pitch, entity.rotation.pitch, partialTicks);
    const double yawRadians = static_cast<double>(yaw) * M_PI / 180.0;
    const double pitchRadians = static_cast<double>(pitch) * M_PI / 180.0;
    const double pitchCosine = std::cos(pitchRadians);
    return {-std::sin(yawRadians) * pitchCosine, -std::sin(pitchRadians),
            std::cos(yawRadians) * pitchCosine};
}

std::optional<RayHit> intercept(const AABB& box, const glm::dvec3& start, const glm::dvec3& end) {
    const glm::dvec3 delta = end - start;
    double minimum = 0.0;
    double maximum = 1.0;
    uint8_t sideHit = 0;

    const auto clipAxis = [&](const double origin, const double direction,
                              const double lower, const double upper,
                              uint8_t nearFace, uint8_t farFace) {
        if (std::abs(direction) < 1.0E-12) return origin >= lower && origin <= upper;
        double near = (lower - origin) / direction;
        double far = (upper - origin) / direction;
        if (near > far) {
            std::swap(near, far);
            std::swap(nearFace, farFace);
        }
        if (near > minimum) {
            minimum = near;
            sideHit = nearFace;
        }
        maximum = std::min(maximum, far);
        return minimum <= maximum;
    };

    if (!clipAxis(start.x, delta.x, box.minX, box.maxX, 4, 5)
        || !clipAxis(start.y, delta.y, box.minY, box.maxY, 0, 1)
        || !clipAxis(start.z, delta.z, box.minZ, box.maxZ, 2, 3)) return std::nullopt;
    if (maximum < 0.0 || minimum > 1.0) return std::nullopt;

    const double fraction = std::clamp(minimum, 0.0, 1.0);
    const glm::dvec3 hit = start + delta * fraction;
    RayHit result{hit, glm::distance(start, hit)};
    result.sideHit = sideHit;
    return result;
}

std::optional<RayHit> rayTraceBlocks(const World& world, const glm::dvec3& start,
                                     const glm::dvec3& end) {
    constexpr double margin = 1.0E-7;
    const AABB searchBox{
        std::min(start.x, end.x) - margin, std::min(start.y, end.y) - margin,
        std::min(start.z, end.z) - margin, std::max(start.x, end.x) + margin,
        std::max(start.y, end.y) + margin, std::max(start.z, end.z) + margin,
    };
    const std::vector<LocalBlock> candidates = world.getCollidingBlocks(searchBox);
    std::optional<RayHit> closest;
    for (const LocalBlock& candidate : candidates) {
        if (blockId(candidate.block) == blockId(BLOCK_SNOW_LAYER)
            || blockId(candidate.block) == blockId(BLOCK_PISTON_HEAD)
            || BlockUtil::isPane(candidate.block)) continue;
        // fences and walls collide 1.5 blocks high but are selected as a single 1 block high box
        const bool tallPost = BlockUtil::isFence(candidate.block)
            || BlockUtil::isFenceGate(candidate.block)
            || blockId(candidate.block) == blockId(BLOCK_COBBLESTONE_WALL);
        const auto selectable = [&](AABB bounds, const LocalBlock& source) {
            if (tallPost) bounds.maxY = std::min(bounds.maxY, static_cast<double>(source.y) + 1.0);
            return bounds;
        };
        const auto hit = intercept(selectable(candidate.bounds, candidate), start, end);
        if (!hit || (closest && hit->distance >= closest->distance)) continue;

        closest = RayHit{hit->position, hit->distance, {candidate.x, candidate.y, candidate.z}};
        closest->sideHit = hit->sideHit;
        AABB combined{1.0e9, 1.0e9, 1.0e9, -1.0e9, -1.0e9, -1.0e9};
        for (const LocalBlock& part : candidates) {
            if (part.x != candidate.x || part.y != candidate.y || part.z != candidate.z) continue;
            const AABB bounds = selectable(part.bounds, part);
            combined = {std::min(combined.minX, bounds.minX), std::min(combined.minY, bounds.minY),
                std::min(combined.minZ, bounds.minZ), std::max(combined.maxX, bounds.maxX),
                std::max(combined.maxY, bounds.maxY), std::max(combined.maxZ, bounds.maxZ)};
            if (tallPost) continue;
            if (closest->blockBoundsCount >= closest->blockBounds.size()) break;
            closest->blockBounds[closest->blockBoundsCount++] = bounds;
        }
        if (tallPost) { // one outline around the post and all of its arms
            closest->blockBounds[0] = combined;
            closest->blockBoundsCount = 1;
        }
    }

    const int minimumX = static_cast<int>(std::floor(searchBox.minX));
    const int minimumY = std::max(0, static_cast<int>(std::floor(searchBox.minY)));
    const int minimumZ = static_cast<int>(std::floor(searchBox.minZ));
    const int maximumX = static_cast<int>(std::floor(searchBox.maxX));
    const int maximumY = std::min(WORLD_HEIGHT - 1, static_cast<int>(std::floor(searchBox.maxY)));
    const int maximumZ = static_cast<int>(std::floor(searchBox.maxZ));
    for (int x = minimumX; x <= maximumX; ++x) {
        for (int y = minimumY; y <= maximumY; ++y) {
            for (int z = minimumZ; z <= maximumZ; ++z) {
                const Block block = world.getBlockAt(x, y, z);
                AABB visualBounds;
                switch (blockId(block)) {
                    case blockId(BLOCK_IRON_BARS):
                    case blockId(BLOCK_GLASS_PANE):
                    case blockId(BLOCK_STAINED_GLASS_PANE): {
                        const bool north = BlockUtil::canPaneConnectTo(world.getBlockAt(x, y, z - 1));
                        const bool south = BlockUtil::canPaneConnectTo(world.getBlockAt(x, y, z + 1));
                        const bool west = BlockUtil::canPaneConnectTo(world.getBlockAt(x - 1, y, z));
                        const bool east = BlockUtil::canPaneConnectTo(world.getBlockAt(x + 1, y, z));
                        double minX = 7.0 / 16.0;
                        double maxX = 9.0 / 16.0;
                        double minZ = 7.0 / 16.0;
                        double maxZ = 9.0 / 16.0;
                        if ((!west || !east) && (west || east || north || south)) {
                            if (west) minX = 0.0;
                            else if (east) maxX = 1.0;
                        } else {
                            minX = 0.0;
                            maxX = 1.0;
                        }
                        if ((!north || !south) && (west || east || north || south)) {
                            if (north) minZ = 0.0;
                            else if (south) maxZ = 1.0;
                        } else {
                            minZ = 0.0;
                            maxZ = 1.0;
                        }
                        visualBounds = {x + minX, static_cast<double>(y), z + minZ,
                            x + maxX, y + 1.0, z + maxZ};
                        break;
                    }
                    case blockId(BLOCK_SNOW_LAYER): {
                        const double height = static_cast<double>((blockMeta(block) & 7) + 1) / 8.0;
                        visualBounds = {static_cast<double>(x), static_cast<double>(y),
                            static_cast<double>(z), x + 1.0, y + height, z + 1.0};
                        break;
                    }
                    case blockId(BLOCK_PISTON_HEAD): {
                        switch (blockMeta(block) & 7) {
                            case 0: visualBounds = {x + 0.0, y + 0.0, z + 0.0, x + 1.0, y + 0.25, z + 1.0}; break;
                            case 1: visualBounds = {x + 0.0, y + 0.75, z + 0.0, x + 1.0, y + 1.0, z + 1.0}; break;
                            case 2: visualBounds = {x + 0.0, y + 0.0, z + 0.0, x + 1.0, y + 1.0, z + 0.25}; break;
                            case 3: visualBounds = {x + 0.0, y + 0.0, z + 0.75, x + 1.0, y + 1.0, z + 1.0}; break;
                            case 4: visualBounds = {x + 0.0, y + 0.0, z + 0.0, x + 0.25, y + 1.0, z + 1.0}; break;
                            case 5: visualBounds = {x + 0.75, y + 0.0, z + 0.0, x + 1.0, y + 1.0, z + 1.0}; break;
                            default: continue;
                        }
                        break;
                    }
                    case blockId(BLOCK_TALLGRASS):
                    case blockId(BLOCK_DEADBUSH):
                    case blockId(BLOCK_SAPLING):
                        visualBounds = {x + 0.1, static_cast<double>(y), z + 0.1,
                            x + 0.9, y + 0.8, z + 0.9};
                        break;
                    case blockId(BLOCK_YELLOW_FLOWER):
                    case blockId(BLOCK_RED_FLOWER):
                        visualBounds = {x + 0.3, static_cast<double>(y), z + 0.3,
                            x + 0.7, y + 0.6, z + 0.7};
                        break;
                    case blockId(BLOCK_BROWN_MUSHROOM):
                    case blockId(BLOCK_RED_MUSHROOM):
                        visualBounds = {x + 0.3, static_cast<double>(y), z + 0.3,
                            x + 0.7, y + 0.4, z + 0.7};
                        break;
                    case blockId(BLOCK_REEDS):
                        visualBounds = {x + 0.375, static_cast<double>(y), z + 0.375,
                            x + 0.625, y + 1.0, z + 0.625};
                        break;
                    case blockId(BLOCK_WHEAT):
                    case blockId(BLOCK_CARROTS):
                    case blockId(BLOCK_POTATOES):
                        visualBounds = {static_cast<double>(x), static_cast<double>(y),
                            static_cast<double>(z), x + 1.0, y + 0.25, z + 1.0};
                        break;
                    case blockId(BLOCK_PUMPKIN_STEM):
                    case blockId(BLOCK_MELON_STEM): {
                        const double height = static_cast<double>((blockMeta(block) & 7) * 2 + 2) / 16.0;
                        visualBounds = {x + 0.375, static_cast<double>(y), z + 0.375,
                            x + 0.625, y + height, z + 0.625};
                        break;
                    }
                    case blockId(BLOCK_NETHER_WART):
                        visualBounds = {x + 0.2, static_cast<double>(y), z + 0.2,
                            x + 0.8, y + 0.25, z + 0.8};
                        break;
                    case blockId(BLOCK_DOUBLE_PLANT):
                        visualBounds = {x + 0.3, static_cast<double>(y), z + 0.3,
                            x + 0.7, y + 0.6, z + 0.7};
                        break;
                    case blockId(BLOCK_WEB):
                        visualBounds = {static_cast<double>(x), static_cast<double>(y),
                            static_cast<double>(z), x + 1.0, y + 1.0, z + 1.0};
                        break;
                    case blockId(BLOCK_RAIL): case blockId(BLOCK_GOLDEN_RAIL):
                    case blockId(BLOCK_DETECTOR_RAIL): case blockId(BLOCK_ACTIVATOR_RAIL): {
                        // blockRailBase.setBlockBoundsBasedOnState: ascending rails are half a block tall
                        const int shape = blockId(block) == blockId(BLOCK_RAIL) ? blockMeta(block) : blockMeta(block) & 7;
                        const double height = shape >= 2 && shape <= 5 ? 0.5 : 0.125;
                        visualBounds = {static_cast<double>(x), static_cast<double>(y),
                            static_cast<double>(z), x + 1.0, y + height, z + 1.0};
                        break;
                    }
                    case blockId(BLOCK_TORCH): case blockId(BLOCK_REDSTONE_TORCH):
                    case blockId(BLOCK_UNLIT_REDSTONE_TORCH): {
                        // blockTorch.getBoundingBox
                        constexpr double half = 0.15;
                        switch (blockMeta(block)) {
                            case 1: visualBounds = {x + 0.0, y + 0.2, z + 0.5 - half, x + half * 2.0, y + 0.8, z + 0.5 + half}; break;
                            case 2: visualBounds = {x + 1.0 - half * 2.0, y + 0.2, z + 0.5 - half, x + 1.0, y + 0.8, z + 0.5 + half}; break;
                            case 3: visualBounds = {x + 0.5 - half, y + 0.2, z + 0.0, x + 0.5 + half, y + 0.8, z + half * 2.0}; break;
                            case 4: visualBounds = {x + 0.5 - half, y + 0.2, z + 1.0 - half * 2.0, x + 0.5 + half, y + 0.8, z + 1.0}; break;
                            default: visualBounds = {x + 0.4, static_cast<double>(y), z + 0.4, x + 0.6, y + 0.6, z + 0.6}; break;
                        }
                        break;
                    }
                    case blockId(BLOCK_LADDER): {
                        constexpr double thickness = 0.125;
                        switch (blockMeta(block)) {
                            case 2: visualBounds = {static_cast<double>(x), static_cast<double>(y), z + 1.0 - thickness, x + 1.0, y + 1.0, z + 1.0}; break;
                            case 3: visualBounds = {static_cast<double>(x), static_cast<double>(y), static_cast<double>(z), x + 1.0, y + 1.0, z + thickness}; break;
                            case 4: visualBounds = {x + 1.0 - thickness, static_cast<double>(y), static_cast<double>(z), x + 1.0, y + 1.0, z + 1.0}; break;
                            default: visualBounds = {static_cast<double>(x), static_cast<double>(y), static_cast<double>(z), x + thickness, y + 1.0, z + 1.0}; break;
                        }
                        break;
                    }
                    case blockId(BLOCK_STONE_BUTTON): case blockId(BLOCK_WOODEN_BUTTON): {
                        const double depth = (blockMeta(block) & 8) != 0 ? 1.0 / 16.0 : 2.0 / 16.0;
                        switch (blockMeta(block) & 7) {
                            case 1: visualBounds = {static_cast<double>(x), y + 0.375, z + 0.3125,
                                x + depth, y + 0.625, z + 0.6875}; break;
                            case 2: visualBounds = {x + 1.0 - depth, y + 0.375, z + 0.3125,
                                x + 1.0, y + 0.625, z + 0.6875}; break;
                            case 3: visualBounds = {x + 0.3125, y + 0.375, static_cast<double>(z),
                                x + 0.6875, y + 0.625, z + depth}; break;
                            case 4: visualBounds = {x + 0.3125, y + 0.375, z + 1.0 - depth,
                                x + 0.6875, y + 0.625, z + 1.0}; break;
                            default: continue;
                        }
                        break;
                    }
                    case blockId(BLOCK_STANDING_SIGN): {
                        visualBounds = {x + 0.25, static_cast<double>(y), z + 0.25,
                            x + 0.75, y + 1.0, z + 0.75};
                        break;
                    }
                    case blockId(BLOCK_WALL_SIGN): {
                        constexpr double thickness = 2.0 / 16.0;
                        switch (blockMeta(block)) {
                            case 2: visualBounds = {x + 0.0, y + 0.28125, z + 1.0 - thickness, x + 1.0, y + 0.78125, z + 1.0}; break;
                            case 3: visualBounds = {x + 0.0, y + 0.28125, z + 0.0, x + 1.0, y + 0.78125, z + thickness}; break;
                            case 4: visualBounds = {x + 1.0 - thickness, y + 0.28125, z + 0.0, x + 1.0, y + 0.78125, z + 1.0}; break;
                            default: visualBounds = {x + 0.0, y + 0.28125, z + 0.0, x + thickness, y + 0.78125, z + 1.0}; break;
                        }
                        break;
                    }
                    case blockId(BLOCK_FENCE_GATE): case blockId(BLOCK_SPRUCE_FENCE_GATE):
                    case blockId(BLOCK_BIRCH_FENCE_GATE): case blockId(BLOCK_JUNGLE_FENCE_GATE):
                    case blockId(BLOCK_DARK_OAK_FENCE_GATE): case blockId(BLOCK_ACACIA_FENCE_GATE): {
                        if ((blockMeta(block) & 1) == 0) {
                            visualBounds = {static_cast<double>(x), static_cast<double>(y), z + 0.375,
                                x + 1.0, y + 1.0, z + 0.625};
                        } else {
                            visualBounds = {x + 0.375, static_cast<double>(y), static_cast<double>(z),
                                x + 0.625, y + 1.0, z + 1.0};
                        }
                        break;
                    }
                    case blockId(BLOCK_WATERLILY):
                        visualBounds = {x + 0.0625, static_cast<double>(y), z + 0.0625,
                            x + 0.9375, y + 0.015625, z + 0.9375};
                        break;
                    case blockId(BLOCK_REDSTONE_WIRE):
                        visualBounds = {static_cast<double>(x), static_cast<double>(y), static_cast<double>(z),
                            x + 1.0, y + 0.0625, z + 1.0};
                        break;
                    case blockId(BLOCK_UNPOWERED_REPEATER): case blockId(BLOCK_POWERED_REPEATER):
                    case blockId(BLOCK_UNPOWERED_COMPARATOR): case blockId(BLOCK_POWERED_COMPARATOR):
                        visualBounds = {static_cast<double>(x), static_cast<double>(y), static_cast<double>(z),
                            x + 1.0, y + 0.125, z + 1.0};
                        break;
                    case blockId(BLOCK_LEVER): {
                        // BlockLever.setBlockBoundsBasedOnState
                        constexpr double half = 0.25, thick = 0.1875;
                        switch (blockMeta(block) & 7) {
                            case 1: visualBounds = {x + 0.0, y + 0.2, z + 0.5 - half, x + thick, y + 0.8, z + 0.5 + half}; break;
                            case 2: visualBounds = {x + 1.0 - thick, y + 0.2, z + 0.5 - half, x + 1.0, y + 0.8, z + 0.5 + half}; break;
                            case 3: visualBounds = {x + 0.5 - half, y + 0.2, z + 0.0, x + 0.5 + half, y + 0.8, z + thick}; break;
                            case 4: visualBounds = {x + 0.5 - half, y + 0.2, z + 1.0 - thick, x + 0.5 + half, y + 0.8, z + 1.0}; break;
                            case 0: case 7: visualBounds = {x + 0.5 - half, y + 0.4, z + 0.5 - half, x + 0.5 + half, y + 1.0, z + 0.5 + half}; break;
                            default: visualBounds = {x + 0.5 - half, static_cast<double>(y), z + 0.5 - half, x + 0.5 + half, y + 0.6, z + 0.5 + half}; break;
                        }
                        break;
                    }
                    case blockId(BLOCK_STONE_PRESSURE_PLATE): case blockId(BLOCK_WOODEN_PRESSURE_PLATE):
                    case blockId(BLOCK_LIGHT_WEIGHTED_PRESSURE_PLATE): case blockId(BLOCK_HEAVY_WEIGHTED_PRESSURE_PLATE): {
                        // they have no collision, so they are selectable only through this box; pressed ones are lower
                        const double height = blockMeta(block) > 0 ? 0.03125 : 0.0625;
                        visualBounds = {x + 0.0625, static_cast<double>(y), z + 0.0625,
                            x + 0.9375, y + height, z + 0.9375};
                        break;
                    }
                    default:
                        continue;
                }
                const auto hit = intercept(visualBounds, start, end);
                if (!hit || (closest && hit->distance >= closest->distance)) continue;

                closest = RayHit{hit->position, hit->distance, {x, y, z}};
                closest->sideHit = hit->sideHit;
                closest->blockBounds[0] = visualBounds;
                closest->blockBoundsCount = 1;
            }
        }
    }
    return closest;
}
}

// please never let me touch this ever again this is so aids

Entity::Entity(const Minecraft* mc, double x, double y, double z)
    : mc(mc) {
    setPosition(x, y, z);
}

double Entity::getEyePos() const {
    return position.y + getEyeHeight();
}

double Entity::getLastEyePos() const {
    return lastPos.y + getEyeHeight();
}

double Entity::ridingYOffset(const Entity& vehicle) const {
    // entity.getMountedYOffset() is three quarters of the vehicle's height (minecarts: 0); the rider's own offset is -0.35 for players and the zombie/skeleton family
    const bool minecart = vehicle.getType() == MOB && static_cast<const EntityMob&>(vehicle).mobType == 210;
    double riderOffset = 0.0;
    if (getType() == PLAYER) riderOffset = -0.35;
    else if (getType() == MOB) {
        const uint8_t type = static_cast<const EntityMob*>(this)->mobType;
        if (type == 51 || type == 53 || type == 54 || type == 57) riderOffset = -0.35;
    }
    return (minecart ? 0.0 : vehicle.height * 0.75) + riderOffset;
}

bool Entity::updateRidden() {
    if (vehicleId < 0) return false;
    const std::shared_ptr<Entity> vehicle = mc->world->getEntityById(vehicleId);
    if (vehicle == nullptr) {
        vehicleId = -1;
        return false;
    }
    lastPos = position;
    setPositionPreservingPrevious(vehicle->position.x, vehicle->position.y + ridingYOffset(*vehicle),
        vehicle->position.z);
    velocity = {0.0, 0.0, 0.0};
    return true;
}

void Entity::update() {
    lastPos = position;
    lastRot = rotation;
    movedLastTick = false;
    if (hurtTime > 0) --hurtTime;
    if (hurtResistantTime > 0) --hurtResistantTime;
    handleWaterMovement();
    onLivingUpdate();
    if (isSprinting() && !isInWater()) {
        const int blockX = static_cast<int>(std::floor(position.x));
        const int blockY = static_cast<int>(std::floor(position.y - 0.20000000298023224));
        const int blockZ = static_cast<int>(std::floor(position.z));
        const Block block = mc->world->getBlockAt(blockX, blockY, blockZ);
        mc->renderer->addBlockRunningParticle(
            {position.x, getBoundingBox().minY, position.z}, velocity, width, block);
    }
    movedLastTick = lastPos != position;
    firstUpdate = false;
}

void Entity::jump() {
    velocity.y = jumpHeight();
    if (isSprinting() && moveForward > 0.0) {
        constexpr float boost = 0.2F;
        const float yawRadians = rotation.yaw * 0.017453292F;
        velocity.x -= Math::sin(yawRadians) * boost;
        velocity.z += Math::cos(yawRadians) * boost;
    }
    isAirBorne = true;
}

bool Entity::isOnLadder() const {
    if (getType() == PLAYER && ASP(EntityPlayer)->isSpectator()) return false;

    const AABB box = getBoundingBox();
    const int x = static_cast<int>(std::floor(position.x));
    const int y = static_cast<int>(std::floor(box.minY));
    const int z = static_cast<int>(std::floor(position.z));
    return BlockUtil::isClimbable(mc->world->getBlockAt(x, y, z));
}

void Entity::onLivingUpdate() {
    if (jumpTicks > 0) --jumpTicks;

    if (std::abs(velocity.x) < 0.005) velocity.x = 0.0;
    if (std::abs(velocity.y) < 0.005) velocity.y = 0.0;
    if (std::abs(velocity.z) < 0.005) velocity.z = 0.0;

    moveStrafe *= 0.98F;
    moveForward *= 0.98F;

    if (isJumping) {
        if (isInWater()) {
            velocity.y += 0.03999999910593033;
        } else if (isInLava()) {
            velocity.y += 0.03999999910593033;
        } else if (onGround && jumpTicks == 0) {
            jump();
            jumpTicks = 10;
        }
    } else {
        jumpTicks = 0;
    }

    moveEntityWithHeading(moveStrafe, moveForward);

}

Block Entity::getBlockBelow() const {
    return mc->world->getBlockAt(std::floor(position.x), static_cast<int>(std::floor(getBoundingBox().minY)) - 1, std::floor(position.z));
}

void Entity::moveEntityWithHeading(const float strafe, const float forward) {
    if (isInWater()) {
        const double initialY = position.y;
        moveFlying(strafe, forward, 0.02F);
        moveEntity(velocity.x, velocity.y, velocity.z);
        velocity.x *= 0.800000011920929f;
        velocity.y *= 0.800000011920929f;
        velocity.z *= 0.800000011920929f;
        velocity.y -= 0.02;
        if (isCollidedHorizontally && isOffsetPositionInLiquid(velocity.x,
            velocity.y + 0.6000000238418579 - position.y + initialY, velocity.z)) {
            velocity.y = 0.30000001192092896;
        }
        return;
    }
    if (isInLava()) {
        const double initialY = position.y;
        moveFlying(strafe, forward, 0.02F);
        moveEntity(velocity.x, velocity.y, velocity.z);
        velocity.x *= 0.5f;
        velocity.y *= 0.5f;
        velocity.z *= 0.5f;
        velocity.y -= 0.02;
        if (isCollidedHorizontally && isOffsetPositionInLiquid(velocity.x,
            velocity.y + 0.6000000238418579 - position.y + initialY, velocity.z)) {
            velocity.y = 0.30000001192092896;
        }
        return;
    }

    float f4 = 0.91f;

    if (onGround) {
        f4 = BlockUtil::getSlipperiness(getBlockBelow()) * 0.91f;
    }

    const float f = 0.16277136F / (f4 * f4 * f4);
    const float f5 = onGround ? getAIMoveSpeed() * f : jumpMovementFactor;

    moveFlying(strafe, forward, f5);
    f4 = 0.91f;

    if (onGround) {
        f4 = BlockUtil::getSlipperiness(getBlockBelow()) * 0.91f;
    }

    if (isOnLadder()) {
        constexpr float f6 = 0.15f;
        velocity.x = std::clamp(velocity.x, static_cast<double>(-f6), static_cast<double>(f6));
        velocity.z = std::clamp(velocity.z, static_cast<double>(-f6), static_cast<double>(f6));
        fallDistance = 0.0f;

        if (velocity.y < -0.15) {
            velocity.y = -0.15;
        }

        if (isSneaking() && getType() == PLAYER && velocity.y < 0.0) {
            velocity.y = 0.0;
        }
    }

    moveEntity(velocity.x, velocity.y, velocity.z);

    if (isCollidedHorizontally && isOnLadder()) {
        velocity.y = 0.2;
    }

    if (!mc->world->isChunkAtLoaded(position.x, position.z)) {
        if (position.y > 0.0) {
            velocity.y = -0.1;
        } else {
            velocity.y = 0.0;
        }
    } else {
        velocity.y -= 0.08;
    }

    velocity.y *= 0.98f;
    velocity.x *= static_cast<double>(f4);
    velocity.z *= static_cast<double>(f4);
}

void Entity::moveFlying(float strafe, float forward, const float friction) const {
    float f = strafe * strafe + forward * forward;
    if (f < 1e-4) return;
    f = std::sqrt(f);
    if (f < 1.0f) {
        f = 1.0f;
    }
    f = friction / f;
    strafe *= f;
    forward *= f;

    const float yawRadians = rotation.yaw * 0.017453292F;
    const float f1 = Math::sin(yawRadians);
    const float f2 = Math::cos(yawRadians);
    velocity.x += strafe * f2 - forward * f1;
    velocity.z += forward * f2  + strafe * f1;
}

void Entity::moveEntity(double dx, double dy, double dz) {
    if (noClip) {
        boundingBox = getBoundingBox().offset(dx, dy, dz);
        resetPositionToBB();
        return;
    }

    if (isInWeb)
    {
        isInWeb = false;
        dx *= 0.25;
        dy *= 0.05000000074505806;
        dz *= 0.25;
        velocity.x = 0.0;
        velocity.y = 0.0;
        velocity.z = 0.0;
    }

    double d3 = dx;
    double d4 = dy;
    double d5 = dz;

    if (onGround && isSneaking() && getType() == PLAYER) {

        double d6;
        for (d6 = 0.05; dx != 0.0 && mc->world->getCollidingBlocks(getBoundingBox().offset(dx, -1.0, 0.0)).empty(); d3 = dx)
        {
            if (dx < d6 && dx >= -d6)
            {
                dx = 0.0;
            }
            else if (dx > 0.0)
            {
                dx -= d6;
            }
            else
            {
                dx += d6;
            }
        }

        for (; dz != 0.0 && mc->world->getCollidingBlocks(getBoundingBox().offset(0.0, -1.0, dz)).empty(); d5 = dz)
        {
            if (dz < d6 && dz >= -d6)
            {
                dz = 0.0;
            }
            else if (dz > 0.0)
            {
                dz -= d6;
            }
            else
            {
                dz += d6;
            }
        }

        for (; dx != 0.0 && dz != 0.0 && mc->world->getCollidingBlocks(getBoundingBox().offset(dx, -1.0, dz)).empty(); d5 = dz)
        {
            if (dx < d6 && dx >= -d6)
            {
                dx = 0.0;
            }
            else if (dx > 0.0)
            {
                dx -= d6;
            }
            else
            {
                dx += d6;
            }

            d3 = dx;

            if (dz < d6 && dz >= -d6)
            {
                dz = 0.0;
            }
            else if (dz > 0.0)
            {
                dz -= d6;
            }
            else
            {
                dz += d6;
            }
        }
    }

    auto list1 = mc->world->getCollidingBlocks(getBoundingBox().expand(dx, dy, dz));
    auto axisalignedbb = getBoundingBox();

    for (auto axisalignedbb1 : list1)
    {
        dy = axisalignedbb1.calculateYOffset(getBoundingBox(), dy);
    }

    setBoundingBox(getBoundingBox().offset(0.0, dy, 0.0));
    bool flag1 = onGround || d4 != dy && d4 < 0.0;

    for (auto axisalignedbb2 : list1)
    {
        dx = axisalignedbb2.calculateXOffset(getBoundingBox(), dx);
    }

    setBoundingBox(getBoundingBox().offset(dx, 0.0, 0.0));

    for (auto axisalignedbb13 : list1)
    {
        dz = axisalignedbb13.calculateZOffset(getBoundingBox(), dz);
    }

    setBoundingBox(getBoundingBox().offset(0.0, 0.0, dz));

    if (stepHeight > 0.0F && flag1 && (d3 != dx || d5 != dz))
    {
        double d11 = dx;
        double d7 = dy;
        double d8 = dz;
        auto axisalignedbb3 = getBoundingBox();
        boundingBox = axisalignedbb;
        dy = static_cast<double>(stepHeight);
        auto list = mc->world->getCollidingBlocks(getBoundingBox().expand(d3, dy, d5));
        auto axisalignedbb4 = getBoundingBox();
        auto axisalignedbb5 = axisalignedbb4.expand(d3, 0.0, d5);
        double d9 = dy;

        for (auto axisalignedbb6 : list)
        {
            d9 = axisalignedbb6.calculateYOffset(axisalignedbb5, d9);
        }

        axisalignedbb4 = axisalignedbb4.offset(0.0, d9, 0.0);
        double d15 = d3;

        for (auto axisalignedbb7 : list)
        {
            d15 = axisalignedbb7.calculateXOffset(axisalignedbb4, d15);
        }

        axisalignedbb4 = axisalignedbb4.offset(d15, 0.0, 0.0);
        double d16 = d5;

        for (auto axisalignedbb8 : list)
        {
            d16 = axisalignedbb8.calculateZOffset(axisalignedbb4, d16);
        }

        axisalignedbb4 = axisalignedbb4.offset(0.0, 0.0, d16);
        auto axisalignedbb14 = getBoundingBox();
        double d17 = dy;

        for (auto axisalignedbb9 : list)
        {
            d17 = axisalignedbb9.calculateYOffset(axisalignedbb14, d17);
        }

        axisalignedbb14 = axisalignedbb14.offset(0.0, d17, 0.0);
        double d18 = d3;

        for (auto axisalignedbb10 : list)
        {
            d18 = axisalignedbb10.calculateXOffset(axisalignedbb14, d18);
        }

        axisalignedbb14 = axisalignedbb14.offset(d18, 0.0, 0.0);
        double d19 = d5;

        for (auto axisalignedbb11 : list)
        {
            d19 = axisalignedbb11.calculateZOffset(axisalignedbb14, d19);
        }

        axisalignedbb14 = axisalignedbb14.offset(0.0, 0.0, d19);
        double d20 = d15 * d15 + d16 * d16;
        double d10 = d18 * d18 + d19 * d19;

        if (d20 > d10)
        {
            dx = d15;
            dz = d16;
            dy = -d9;
            setBoundingBox(axisalignedbb4);
        }
        else
        {
            dx = d18;
            dz = d19;
            dy = -d17;
            setBoundingBox(axisalignedbb14);
        }

        for (auto axisalignedbb12 : list)
        {
            dy = axisalignedbb12.calculateYOffset(getBoundingBox(), dy);
        }

        setBoundingBox(getBoundingBox().offset(0.0, dy, 0.0));

        if (d11 * d11 + d8 * d8 >= dx * dx + dz * dz)
        {
            dx = d11;
            dy = d7;
            dz = d8;
            setBoundingBox(axisalignedbb3);
        }
    }

    resetPositionToBB();
    isCollidedHorizontally = d3 != dx || d5 != dz;
    isCollidedVertically = d4 != dy;
    onGround = isCollidedVertically && d4 < 0.0;
    isCollided = isCollidedVertically || isCollidedHorizontally;

    bool slimeHandledVerticalCollision = false;
    if (d4 != dy) {
        // entity.moveEntity calls onLanded on the block below the post-collision position even for an upward head collision. BlockSlime only changes negative motion and, unlike the base implementation, deliberately leaves positive motion untouched
        const Block landedBlock = mc->world->getBlockAt(
            static_cast<int>(std::floor(position.x)),
            static_cast<int>(std::floor(position.y - 0.20000000298023224)),
            static_cast<int>(std::floor(position.z)));
        if (blockId(landedBlock) == blockId(BLOCK_SLIME) && !isSneaking()) {
            if (velocity.y < 0.0) velocity.y = -velocity.y;
            slimeHandledVerticalCollision = true;
        }
    }

    // entityLivingBase.updateFallState refreshes water immediately when this movement enters it
    if ((getType() == PLAYER || getType() == MOB) && !isInWater()) handleWaterMovement();

    if (onGround) {
        if (fallDistance > 3.0f && getType() == PLAYER) {
            const int blockX = static_cast<int>(std::floor(position.x));
            const int blockY = static_cast<int>(std::floor(position.y - 0.2));
            const int blockZ = static_cast<int>(std::floor(position.z));
            const Block landedOn = mc->world->getBlockAt(blockX, blockY, blockZ);
            if (blockId(landedOn) != blockId(BLOCK_AIR)) {
                const float strength = std::min(0.2f + std::ceil(fallDistance - 3.0f) / 15.0f, 2.5f);
                mc->renderer->addBlockLandingParticles(position, width, landedOn,
                    static_cast<int>(150.0f * strength));
            }
        }
        fallDistance = 0.0f;
    } else if (dy < 0.0) {
        fallDistance -= static_cast<float>(dy);
    }

    if (d3 != dx)
    {
        velocity.x = 0.0;
    }

    if (d5 != dz)
    {
        velocity.z = 0.0;
    }

    if (d4 != dy && !slimeHandledVerticalCollision) {
        velocity.y = 0.0;
    }

    // entity.doBlockCollisions. These blocks have no collision box of their own; their movement behavior is applied after this move and therefore affects the following movement tick
    const AABB collisionBounds = getBoundingBox().contract(0.001, 0.001, 0.001);
    const int minX = static_cast<int>(std::floor(collisionBounds.minX));
    const int maxX = static_cast<int>(std::floor(collisionBounds.maxX));
    const int minY = static_cast<int>(std::floor(collisionBounds.minY));
    const int maxY = static_cast<int>(std::floor(collisionBounds.maxY));
    const int minZ = static_cast<int>(std::floor(collisionBounds.minZ));
    const int maxZ = static_cast<int>(std::floor(collisionBounds.maxZ));
    for (int blockX = minX; blockX <= maxX; ++blockX) {
        for (int blockY = minY; blockY <= maxY; ++blockY) {
            for (int blockZ = minZ; blockZ <= maxZ; ++blockZ) {
                switch (blockId(mc->world->getBlockAt(blockX, blockY, blockZ))) {
                    case blockId(BLOCK_WEB):
                        isInWeb = true;
                        fallDistance = 0.0f;
                        break;
                    case blockId(BLOCK_SOUL_SAND):
                        velocity.x *= 0.4;
                        velocity.z *= 0.4;
                        break;
                    case blockId(BLOCK_SLIME): {
                        if (isSneaking()) break;
                        const double factor = 0.4 + std::abs(velocity.y) * 0.2;
                        velocity.x *= factor;
                        velocity.z *= factor;
                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }

}

void Entity::resetPositionToBB() const {
    position.x = (boundingBox.minX + boundingBox.maxX) / 2;
    position.y = boundingBox.minY;
    position.z = (boundingBox.minZ + boundingBox.maxZ) / 2;
}

AABB Entity::getBoundingBox() const {
    return boundingBox;
}
void Entity::setBoundingBox(const AABB &box) const {
    boundingBox = box;
}

void Entity::setPosition(const double x, const double y, const double z) const {
    lastPos.x = position.x = x;
    lastPos.y = position.y = y;
    lastPos.z = position.z = z;
    lastPos = position;
    boundingBox = AABB{position.x - width/2, position.y, position.z - width/2, position.x + width/2, position.y + height, position.z + width/2};
}
void Entity::setPositionPreservingPrevious(const double x, const double y, const double z) const {
    position = {x, y, z};
    boundingBox = AABB{position.x - width / 2, position.y, position.z - width / 2,
        position.x + width / 2, position.y + height, position.z + width / 2};
}
void Entity::setRotation(const float yaw, const float pitch) const {
    lastRot.yaw = rotation.yaw = std::fmod(yaw, 360.0f);
    lastRot.pitch = rotation.pitch = std::fmod(pitch, 360.0f);
}
void Entity::setPositionAndRotation(const double x, const double y, const double z, const float yaw, const float pitch) const {
    setPosition(x, y, z);
    setRotation(yaw, pitch);
}

bool Entity::isCurrentViewEntity() const {
    return mc->getRenderViewEntity().get() == this;
}

glm::vec<3, double> Entity::getPosEyes(const float partialTicks) const {
    const double x = std::lerp(lastPos.x, position.x, static_cast<double>(partialTicks));
    const double y = std::lerp(lastPos.y, position.y, static_cast<double>(partialTicks));
    const double z = std::lerp(lastPos.z, position.z, static_cast<double>(partialTicks));
    return { x, y + getEyeHeight(), z };
}

void Entity::getMouseOver(const float partialTicks) const {
    MovingObjectPosition& mouseOver = mc->objectMouseOver;
    mouseOver.clear();
    mc->pointedEntity = nullptr;

    const std::shared_ptr<Entity> entity = mc->getRenderViewEntity();
    if (entity == nullptr || mc->world == nullptr) return;

    const bool extendedReach = mc->player->gameType == GameType::CREATIVE;
    double reachDistance = extendedReach ? 6.0 : 4.5;
    double entityReach = reachDistance;
    const bool limitEntityReach = !extendedReach && reachDistance > 3.0;
    const glm::dvec3 eyes = entity->getPosEyes(partialTicks);
    const glm::dvec3 look = lookVector(*entity, partialTicks);
    const glm::dvec3 rayEnd = eyes + look * reachDistance;

    if (const auto blockHit = rayTraceBlocks(*mc->world, eyes, rayEnd)) {
        mouseOver.valid = true;
        mouseOver.typeOfHit = MovingObjectPosition::BLOCK;
        mouseOver.hitVec = blockHit->position;
        mouseOver.sideHit = blockHit->sideHit;
        mouseOver.blockPos = blockHit->blockPosition;
        mouseOver.blockBounds = blockHit->blockBounds;
        mouseOver.blockBoundsCount = blockHit->blockBoundsCount;
        entityReach = blockHit->distance;
    }

    Entity* hoveredEntity = nullptr;
    glm::dvec3 entityHitPosition{};
    double closestEntityDistance = entityReach;
    const AABB sweptBox = entity->getBoundingBox().expand(
        look.x * reachDistance, look.y * reachDistance, look.z * reachDistance);
    const AABB searchBox{sweptBox.minX - 1.0, sweptBox.minY - 1.0, sweptBox.minZ - 1.0,
                         sweptBox.maxX + 1.0, sweptBox.maxY + 1.0, sweptBox.maxZ + 1.0};
    for (const std::shared_ptr<Entity>& candidate : mc->world->snapshotEntities()) {
        if (candidate == nullptr || candidate.get() == entity.get()) continue;
        if (candidate->getType() != PLAYER && candidate->getType() != MOB) continue;
        if (candidate->getType() == MOB) {
            const auto mob = static_cast<const EntityMob*>(candidate.get());
            if (mob->hasArmorStandMarker()) continue;
        }
        constexpr double border = 0.1;
        const AABB bounds = candidate->getBoundingBox();
        const AABB expanded{bounds.minX - border, bounds.minY - border, bounds.minZ - border,
                            bounds.maxX + border, bounds.maxY + border, bounds.maxZ + border};
        if (!expanded.intersects(searchBox)) continue;

        const auto hit = intercept(expanded, eyes, rayEnd);
        if (!hit || (hit->distance >= closestEntityDistance && closestEntityDistance != 0.0)) continue;
        if (candidate.get() == entity->ridingEntity && closestEntityDistance != 0.0) continue;
        hoveredEntity = candidate.get();
        entityHitPosition = hit->position;
        closestEntityDistance = hit->distance;
    }

    if (hoveredEntity != nullptr && limitEntityReach
        && glm::distance(eyes, entityHitPosition) > 3.0) {
        mouseOver.valid = true;
        mouseOver.typeOfHit = MovingObjectPosition::MISS;
        mouseOver.hitVec = entityHitPosition;
        mouseOver.blockPos = glm::ivec3(glm::floor(entityHitPosition));
        return;
    }

    if (hoveredEntity != nullptr && (closestEntityDistance < entityReach || !mouseOver.valid)) {
        mouseOver.valid = true;
        mouseOver.typeOfHit = MovingObjectPosition::ENTITY;
        mouseOver.hitVec = entityHitPosition;
        mouseOver.entityHit = hoveredEntity;
        mc->pointedEntity = hoveredEntity;
    }
}

bool Entity::isInLava() const {
    return containsMaterial(*mc->world,
        getBoundingBox().contract(0.10000000149011612, 0.4000000059604645, 0.10000000149011612), false);
}

bool Entity::isInWater() const {
    return inWater;
}

bool Entity::handleWaterMovement() {
    const AABB waterBounds = getBoundingBox().contract(0.0, 0.4000000059604645, 0.0)
        .contract(0.001, 0.001, 0.001);
    if (accelerateInWater(*mc->world, waterBounds, *this)) {
        if (!inWater && !firstUpdate) resetHeight();

        fallDistance = 0.0f;
        inWater = true;
        fire = 0;
    } else {
        inWater = false;
    }

    return inWater;
}

bool Entity::isWet() const {
    return inWater;
}

void Entity::resetHeight() {
    // todo
}

bool Entity::isOffsetPositionInLiquid(const double x, const double y, const double z) const {
    const AABB offsetBounds = getBoundingBox().offset(x, y, z);
    if (!mc->world->getCollidingBlocks(offsetBounds).empty()) return false;
    return !containsMaterial(*mc->world, offsetBounds, true)
        && !containsMaterial(*mc->world, offsetBounds, false);
}
