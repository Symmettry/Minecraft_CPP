#define INT static_cast<int>
#define FLOAT static_cast<float>

#include "World.hpp"
#include "chunk/Block.hpp"
#include "net/lily/minecpp/Minecraft.hpp"

void World::setBlockAt(const int worldX, const int worldY, const int worldZ, const Block value) const {
    if (worldY < 0 || worldY >= WORLD_HEIGHT) return;
    if (blockId(value) != blockId(BLOCK_SKULL)) {
        std::unique_lock blockEntityLock(blockEntityMutex);
        blockEntities.erase(positionKey({worldX, worldY, worldZ}));
    }

    const int chunkX = static_cast<int>(std::floor(static_cast<double>(worldX) / CHUNK_SIZE));
    const int chunkZ = static_cast<int>(std::floor(static_cast<double>(worldZ) / CHUNK_SIZE));
    const int localX = toLocalX(worldX);
    const int localZ = toLocalZ(worldZ);
    const std::shared_ptr<Chunk> chunk = getLoadedChunk(chunkX, chunkZ);
    if (chunk == nullptr) return;

    uint32_t lightSections = 0;
    {
        std::lock_guard lock(chunk->meshMutex);
        chunk->setBlock(localX, worldY, localZ, value);
        lightSections = chunk->relightSkylightSections();
    }
    queueBlockChangeMesh(chunkX, chunkZ, localX, worldY, localZ, lightSections);
}

// a block's faces and lighting depend on the blocks around it, one block either side
uint32_t World::sectionsAround(const int worldY) {
    uint32_t sections = 0;
    for (int section = std::max(worldY - 1, 0) / CHUNK_SIZE;
         section <= std::min(worldY + 1, WORLD_HEIGHT - 1) / CHUNK_SIZE; ++section)
        sections |= 1U << section;
    return sections;
}

void World::queueBlockChangeMesh(const int chunkX, const int chunkZ, const int localX, const int worldY,
                                 const int localZ, const uint32_t lightSections) const {
    const uint32_t blockSections = sectionsAround(worldY);
    const auto queue = [&](const int x, const int z, const uint32_t sections) {
        if (const std::shared_ptr<Chunk> changed = getLoadedChunk(x, z)) {
            changed->queueMesh(mc->renderer->blockAtlas, true, sections); // edits jump the loading queue
        }
    };
    queue(chunkX, chunkZ, blockSections | lightSections);
    if (localX == 0) queue(chunkX - 1, chunkZ, blockSections);
    else if (localX == CHUNK_SIZE - 1) queue(chunkX + 1, chunkZ, blockSections);
    if (localZ == 0) queue(chunkX, chunkZ - 1, blockSections);
    else if (localZ == CHUNK_SIZE - 1) queue(chunkX, chunkZ + 1, blockSections);
}

void World::update() const {
    ++totalTime;
    if (daylightCycle) ++worldTime;
    const std::vector<std::shared_ptr<Entity>> entitySnapshot = snapshotEntities();
    for (const auto& entity : entitySnapshot) {
        entity->update();
    }

    std::vector<PistonMovement> movements;
    {
        std::lock_guard lock(blockEventMutex);
        movements = pistonMovements;
        for (PistonMovement& movement : pistonMovements) ++movement.tick;
        std::erase_if(pistonMovements, [](const PistonMovement& movement) { return movement.tick >= 3; });
    }
    for (const PistonMovement& movement : movements) {
        const double distance = movement.tick < 2 ? 0.5625 : 0.25;
        const glm::dvec3 direction = movement.direction;
        const glm::dvec3 destination = glm::dvec3(movement.position + movement.direction);
        const double progress = movement.tick == 0 ? 0.5 : 1.0;
        AABB movingBounds(destination.x, destination.y, destination.z,
            destination.x + 1.0, destination.y + 1.0, destination.z + 1.0);
        const double clipped = 1.0 - progress;
        if (direction.x > 0) movingBounds.maxX -= clipped;
        else if (direction.x < 0) movingBounds.minX += clipped;
        else if (direction.y > 0) movingBounds.maxY -= clipped;
        else if (direction.y < 0) movingBounds.minY += clipped;
        else if (direction.z > 0) movingBounds.maxZ -= clipped;
        else if (direction.z < 0) movingBounds.minZ += clipped;
        movingBounds = movingBounds.expand(-direction.x * 0.0625, -direction.y * 0.0625,
            -direction.z * 0.0625);
        const auto push = [&](const std::shared_ptr<Entity>& entity) {
            if (entity != nullptr && !entity->noClip && entity->getBoundingBox().intersects(movingBounds))
                entity->moveEntity(direction.x * distance, direction.y * distance, direction.z * distance);
        };
        push(mc->player);
        for (const std::shared_ptr<Entity>& entity : entitySnapshot)
            if (entity != mc->player) push(entity);
    }
}

void World::queuePistonMovement(const glm::ivec3& position, const uint8_t facing) const {
    glm::ivec3 direction{};
    switch (facing & 7) {
        case 0: direction.y = -1; break;
        case 1: direction.y = 1; break;
        case 2: direction.z = -1; break;
        case 3: direction.z = 1; break;
        case 4: direction.x = -1; break;
        case 5: direction.x = 1; break;
        default: return;
    }
    std::lock_guard lock(blockEventMutex);
    pistonMovements.push_back({position, direction, 0});
}

std::shared_ptr<Entity> World::getEntityById(const int entityId) const {
    std::lock_guard lock(entityMutex);
    const auto found = std::ranges::find_if(entities, [entityId](const auto& entity) {
        return entity->entityId == entityId;
    });
    return found == entities.end() ? nullptr : *found;
}

void World::addEntity(const std::shared_ptr<Entity>& entity) const {
    std::lock_guard lock(entityMutex);
    std::erase_if(entities, [&](const auto& current) { return current->entityId == entity->entityId; });
    entities.push_back(entity);
}

void World::removeEntity(const int entityId) const {
    std::lock_guard lock(entityMutex);
    std::erase_if(entities, [entityId](const auto& entity) { return entity->entityId == entityId; });
}

std::vector<std::shared_ptr<Entity>> World::snapshotEntities() const {
    std::lock_guard lock(entityMutex);
    return entities;
}

bool World::hasEntityCollision(const AABB& bounds) const {
    std::lock_guard lock(entityMutex);
    for (const std::shared_ptr<Entity>& entity : entities) {
        if (entity == nullptr || entity->noClip || entity->getType() == ITEM) continue;
        if (entity->getBoundingBox().intersects(bounds)) return true;
    }
    return false;
}

const Block World::airBlock = BLOCK_AIR;

namespace {
struct CollisionShape {
    std::array<AABB, 5> boxes{};
    uint8_t count = 0;

    void add(const double minX, const double minY, const double minZ,
             const double maxX, const double maxY, const double maxZ) {
        boxes[count++] = {minX, minY, minZ, maxX, maxY, maxZ};
    }
};

bool hasNoCollision(const Block block) {
    switch (blockId(block)) {
        case blockId(BLOCK_AIR): case blockId(BLOCK_FLOWING_WATER): case blockId(BLOCK_WATER):
        case blockId(BLOCK_FLOWING_LAVA): case blockId(BLOCK_LAVA): case blockId(BLOCK_SAPLING):
        case blockId(BLOCK_WEB): case blockId(BLOCK_TALLGRASS): case blockId(BLOCK_DEADBUSH):
        case blockId(BLOCK_YELLOW_FLOWER): case blockId(BLOCK_RED_FLOWER):
        case blockId(BLOCK_BROWN_MUSHROOM): case blockId(BLOCK_RED_MUSHROOM):
        case blockId(BLOCK_TORCH): case blockId(BLOCK_FIRE): case blockId(BLOCK_REDSTONE_WIRE):
        case blockId(BLOCK_WHEAT): case blockId(BLOCK_STANDING_SIGN):
        case blockId(BLOCK_RAIL): case blockId(BLOCK_WALL_SIGN): case blockId(BLOCK_LEVER):
        case blockId(BLOCK_UNLIT_REDSTONE_TORCH): case blockId(BLOCK_REDSTONE_TORCH):
        case blockId(BLOCK_STONE_BUTTON): case blockId(BLOCK_WOODEN_BUTTON):
        case blockId(BLOCK_REEDS): case blockId(BLOCK_PORTAL): case blockId(BLOCK_PUMPKIN_STEM):
        case blockId(BLOCK_MELON_STEM): case blockId(BLOCK_VINE): case blockId(BLOCK_NETHER_WART):
        case blockId(BLOCK_END_PORTAL): case blockId(BLOCK_TRIPWIRE_HOOK): case blockId(BLOCK_TRIPWIRE):
        case blockId(BLOCK_CARROTS): case blockId(BLOCK_POTATOES): case blockId(BLOCK_DOUBLE_PLANT):
        case blockId(BLOCK_STANDING_BANNER): case blockId(BLOCK_WALL_BANNER):
        case blockId(BLOCK_GOLDEN_RAIL): case blockId(BLOCK_DETECTOR_RAIL):
        case blockId(BLOCK_ACTIVATOR_RAIL): case blockId(BLOCK_STONE_PRESSURE_PLATE):
        case blockId(BLOCK_WOODEN_PRESSURE_PLATE):
        case blockId(BLOCK_LIGHT_WEIGHTED_PRESSURE_PLATE):
        case blockId(BLOCK_HEAVY_WEIGHTED_PRESSURE_PLATE):
        case blockId(BLOCK_STRUCTURE_VOID):
            return true;
        default:
            return false;
    }
}

bool isFence(const Block block) {
    switch (blockId(block)) {
        case blockId(BLOCK_FENCE): case blockId(BLOCK_NETHER_BRICK_FENCE):
        case blockId(BLOCK_SPRUCE_FENCE): case blockId(BLOCK_BIRCH_FENCE):
        case blockId(BLOCK_JUNGLE_FENCE): case blockId(BLOCK_DARK_OAK_FENCE):
        case blockId(BLOCK_ACACIA_FENCE): return true;
        default: return false;
    }
}

bool isFenceGate(const Block block) {
    switch (blockId(block)) {
        case blockId(BLOCK_FENCE_GATE): case blockId(BLOCK_SPRUCE_FENCE_GATE):
        case blockId(BLOCK_BIRCH_FENCE_GATE): case blockId(BLOCK_JUNGLE_FENCE_GATE):
        case blockId(BLOCK_DARK_OAK_FENCE_GATE): case blockId(BLOCK_ACACIA_FENCE_GATE): return true;
        default: return false;
    }
}

bool canFenceConnect(const Block fence, const Block neighbor) {
    return blockId(fence) == blockId(neighbor) || isFenceGate(neighbor)
        || BlockUtil::hasFullCubeCollision(neighbor);
}

bool canWallConnect(const Block neighbor) {
    return blockId(neighbor) == blockId(BLOCK_COBBLESTONE_WALL) || isFenceGate(neighbor)
        || BlockUtil::hasFullCubeCollision(neighbor);
}

enum class CollisionKind : uint8_t {
    None, Full, Slab, Stair, Snow, Carpet, Farmland, SoulSand, Cactus, Cake, Bed,
    EnchantingTable, Chest, Waterlily, ThinFloor, Door, Trapdoor, FenceGate, Fence,
    Pane, Cauldron, FlowerPot, Skull, EndPortalFrame, DaylightDetector, Anvil, Ladder, PistonHead
};

CollisionKind collisionKind(const Block block) {
    if (hasNoCollision(block)) return CollisionKind::None;
    switch (blockId(block)) {
        case blockId(BLOCK_OAK_STAIRS): case blockId(BLOCK_STONE_STAIRS):
        case blockId(BLOCK_BRICK_STAIRS): case blockId(BLOCK_STONE_BRICK_STAIRS):
        case blockId(BLOCK_NETHER_BRICK_STAIRS): case blockId(BLOCK_SANDSTONE_STAIRS):
        case blockId(BLOCK_SPRUCE_STAIRS): case blockId(BLOCK_BIRCH_STAIRS):
        case blockId(BLOCK_JUNGLE_STAIRS): case blockId(BLOCK_QUARTZ_STAIRS):
        case blockId(BLOCK_ACACIA_STAIRS): case blockId(BLOCK_DARK_OAK_STAIRS):
        case blockId(BLOCK_RED_SANDSTONE_STAIRS): return CollisionKind::Stair;
        case blockId(BLOCK_WOODEN_DOOR): case blockId(BLOCK_IRON_DOOR):
        case blockId(BLOCK_SPRUCE_DOOR): case blockId(BLOCK_BIRCH_DOOR):
        case blockId(BLOCK_JUNGLE_DOOR): case blockId(BLOCK_ACACIA_DOOR):
        case blockId(BLOCK_DARK_OAK_DOOR): return CollisionKind::Door;
        case blockId(BLOCK_FENCE_GATE): case blockId(BLOCK_SPRUCE_FENCE_GATE):
        case blockId(BLOCK_BIRCH_FENCE_GATE): case blockId(BLOCK_JUNGLE_FENCE_GATE):
        case blockId(BLOCK_DARK_OAK_FENCE_GATE):
        case blockId(BLOCK_ACACIA_FENCE_GATE): return CollisionKind::FenceGate;
        case blockId(BLOCK_FENCE): case blockId(BLOCK_NETHER_BRICK_FENCE):
        case blockId(BLOCK_SPRUCE_FENCE): case blockId(BLOCK_BIRCH_FENCE):
        case blockId(BLOCK_JUNGLE_FENCE): case blockId(BLOCK_DARK_OAK_FENCE):
        case blockId(BLOCK_ACACIA_FENCE): return CollisionKind::Fence;
        case blockId(BLOCK_STONE_SLAB): case blockId(BLOCK_WOODEN_SLAB):
        case blockId(BLOCK_STONE_SLAB2): return CollisionKind::Slab;
        case blockId(BLOCK_SNOW_LAYER): return CollisionKind::Snow;
        case blockId(BLOCK_CARPET): return CollisionKind::Carpet;
        case blockId(BLOCK_FARMLAND): return CollisionKind::Farmland;
        case blockId(BLOCK_SOUL_SAND): return CollisionKind::SoulSand;
        case blockId(BLOCK_CACTUS): return CollisionKind::Cactus;
        case blockId(BLOCK_CAKE): return CollisionKind::Cake;
        case blockId(BLOCK_BED): return CollisionKind::Bed;
        case blockId(BLOCK_ENCHANTING_TABLE): return CollisionKind::EnchantingTable;
        case blockId(BLOCK_CHEST): case blockId(BLOCK_TRAPPED_CHEST):
        case blockId(BLOCK_ENDER_CHEST): return CollisionKind::Chest;
        case blockId(BLOCK_WATERLILY): return CollisionKind::Waterlily;
        case blockId(BLOCK_UNPOWERED_REPEATER): case blockId(BLOCK_POWERED_REPEATER):
        case blockId(BLOCK_UNPOWERED_COMPARATOR): case blockId(BLOCK_POWERED_COMPARATOR):
            return CollisionKind::ThinFloor; // BlockRedstoneDiode: a 2/16 slab
        case blockId(BLOCK_TRAPDOOR): case blockId(BLOCK_IRON_TRAPDOOR): return CollisionKind::Trapdoor;
        case blockId(BLOCK_COBBLESTONE_WALL): return CollisionKind::Fence;
        case blockId(BLOCK_IRON_BARS): case blockId(BLOCK_GLASS_PANE):
        case blockId(BLOCK_STAINED_GLASS_PANE): return CollisionKind::Pane;
        case blockId(BLOCK_CAULDRON): return CollisionKind::Cauldron;
        case blockId(BLOCK_FLOWER_POT): return CollisionKind::FlowerPot;
        case blockId(BLOCK_SKULL): return CollisionKind::Skull;
        case blockId(BLOCK_END_PORTAL_FRAME): return CollisionKind::EndPortalFrame;
        case blockId(BLOCK_DAYLIGHT_DETECTOR):
        case blockId(BLOCK_DAYLIGHT_DETECTOR_INVERTED): return CollisionKind::DaylightDetector;
        case blockId(BLOCK_ANVIL): return CollisionKind::Anvil;
        case blockId(BLOCK_LADDER): return CollisionKind::Ladder;
        case blockId(BLOCK_PISTON_HEAD): return CollisionKind::PistonHead;
        default: return CollisionKind::Full;
    }
}
}

std::vector<LocalBlock> World::getCollidingBlocks(const AABB &box) const {
    std::vector<LocalBlock> collisions;
    collisions.reserve(16);

    for (int x = floor(box.minX); x <= box.maxX; ++x) {
        // world.getCollidingBoundingBoxes starts one cell lower: fences and walls reach into the cell above
        for (int y = floor(box.minY - 1.0); y <= box.maxY; ++y) {
            if (y < 0 || y >= WORLD_HEIGHT) continue;
            for (int z = floor(box.minZ); z <= box.maxZ; ++z) {
                const Block block = getBlockAt(x, y, z);
                CollisionShape shape;
                const uint8_t metadata = blockMeta(block);

                switch (collisionKind(block)) {
                case CollisionKind::None:
                    continue;
                case CollisionKind::Slab:
                    if ((metadata & 8) != 0) shape.add(0, 0.5, 0, 1, 1, 1);
                    else shape.add(0, 0, 0, 1, 0.5, 1);
                    break;
                case CollisionKind::Stair: {
                    const bool upsideDown = (metadata & 4) != 0;
                    shape.add(0, upsideDown ? 0.5 : 0, 0, 1, upsideDown ? 1 : 0.5, 1);
                    const uint8_t upper = BlockUtil::stairUpperQuadrants(block, [&](const int dx, const int dz) {
                        return getBlockAt(x + dx, y, z + dz);
                    });
                    for (int quadrant = 0; quadrant < 4; ++quadrant) {
                        if ((upper >> quadrant & 1) == 0) continue;
                        const double minX = (quadrant & 1) * 0.5, minZ = (quadrant >> 1) * 0.5;
                        shape.add(minX, upsideDown ? 0 : 0.5, minZ, minX + 0.5, upsideDown ? 0.5 : 1, minZ + 0.5);
                    }
                    break;
                }
                case CollisionKind::Snow: {
                    const uint8_t layers = metadata & 7;
                    if (layers != 0) {
                        // the normal 1.8 collision is one layer below the visual height. At the maximum layer that leaves a gap beneath an occupied cell above; a zero-layer snow block there otherwise lets the player enter the stack
                        const Block above = getBlockAt(x, y + 1, z);
                        const double height = layers == 7 && blockId(above) != blockId(BLOCK_AIR)
                            ? 1.0 : static_cast<double>(layers) / 8.0;
                        shape.add(0, 0, 0, 1, height, 1);
                    }
                    break;
                }
                case CollisionKind::Carpet:
                    shape.add(0, 0, 0, 1, 1.0 / 16.0, 1);
                    break;
                case CollisionKind::Farmland:
                    shape.add(0, 0, 0, 1, 15.0 / 16.0, 1);
                    break;
                case CollisionKind::SoulSand:
                    shape.add(0, 0, 0, 1, 14.0 / 16.0, 1);
                    break;
                case CollisionKind::Cactus:
                    shape.add(1.0 / 16.0, 0, 1.0 / 16.0,
                        15.0 / 16.0, 15.0 / 16.0, 15.0 / 16.0);
                    break;
                case CollisionKind::Cake:
                    shape.add((1.0 + (metadata & 7) * 2.0) / 16.0, 0, 1.0 / 16.0,
                        15.0 / 16.0, 0.5, 15.0 / 16.0);
                    break;
                case CollisionKind::Bed:
                    shape.add(0, 0, 0, 1, 9.0 / 16.0, 1);
                    break;
                case CollisionKind::EnchantingTable:
                    shape.add(0, 0, 0, 1, 0.75, 1);
                    break;
                case CollisionKind::Chest:
                    shape.add(1.0 / 16.0, 0, 1.0 / 16.0, 15.0 / 16.0, 14.0 / 16.0, 15.0 / 16.0);
                    break;
                case CollisionKind::Waterlily:
                    shape.add(1.0 / 16.0, 0, 1.0 / 16.0, 15.0 / 16.0, 1.0 / 64.0, 15.0 / 16.0);
                    break;
                case CollisionKind::ThinFloor:
                    shape.add(0, 0, 0, 1, 1.0 / 8.0, 1);
                    break;
                case CollisionKind::Door: {
                    constexpr double thickness = 3.0 / 16.0;
                    const Block lower = (metadata & 8) != 0 ? getBlockAt(x, y - 1, z) : block;
                    const Block upper = (metadata & 8) != 0 ? block : getBlockAt(x, y + 1, z);
                    switch (BlockUtil::doorPlane(blockMeta(lower), blockMeta(upper))) {
                        case 0: shape.add(0, 0, 0, 1, 1, thickness); break;
                        case 1: shape.add(1 - thickness, 0, 0, 1, 1, 1); break;
                        case 2: shape.add(0, 0, 1 - thickness, 1, 1, 1); break;
                        case 3: shape.add(0, 0, 0, thickness, 1, 1); break;
                        default: break;
                    }
                    break;
                }
                case CollisionKind::Trapdoor: {
                    constexpr double thickness = 3.0 / 16.0;
                    if ((metadata & 4) == 0) {
                        if ((metadata & 8) != 0) shape.add(0, 1 - thickness, 0, 1, 1, 1);
                        else shape.add(0, 0, 0, 1, thickness, 1);
                    } else {
                        switch (metadata & 3) {
                            case 0: shape.add(0, 0, 1 - thickness, 1, 1, 1); break;
                            case 1: shape.add(0, 0, 0, 1, 1, thickness); break;
                            case 2: shape.add(1 - thickness, 0, 0, 1, 1, 1); break;
                            case 3: shape.add(0, 0, 0, thickness, 1, 1); break;
                            default: break;
                        }
                    }
                    break;
                }
                case CollisionKind::FenceGate:
                    if ((metadata & 4) != 0) continue;
                    if ((metadata & 1) == 0) shape.add(0, 0, 6.0 / 16.0, 1, 1.5, 10.0 / 16.0);
                    else shape.add(6.0 / 16.0, 0, 0, 10.0 / 16.0, 1.5, 1);
                    break;
                case CollisionKind::Fence: {
                    const bool wall = blockId(block) == blockId(BLOCK_COBBLESTONE_WALL);
                    const auto connects = [&](const int nx, const int nz) {
                        const Block neighbor = getBlockAt(nx, y, nz);
                        return wall ? canWallConnect(neighbor) : canFenceConnect(block, neighbor);
                    };
                    const bool west = connects(x - 1, z), east = connects(x + 1, z);
                    const bool north = connects(x, z - 1), south = connects(x, z + 1);
                    if (wall) {
                        double minX = 0.25, maxX = 0.75, minZ = 0.25, maxZ = 0.75;
                        if (west) minX = 0.0;
                        if (east) maxX = 1.0;
                        if (north) minZ = 0.0;
                        if (south) maxZ = 1.0;
                        if (west && east && !north && !south) { minZ = 5.0 / 16.0; maxZ = 11.0 / 16.0; }
                        else if (north && south && !west && !east) { minX = 5.0 / 16.0; maxX = 11.0 / 16.0; }
                        shape.add(minX, 0, minZ, maxX, 1.5, maxZ);
                    } else {
                        if (west || east) shape.add(west ? 0.0 : 6.0 / 16.0, 0, 6.0 / 16.0,
                            east ? 1.0 : 10.0 / 16.0, 1.5, 10.0 / 16.0);
                        if (north || south) shape.add(6.0 / 16.0, 0, north ? 0.0 : 6.0 / 16.0,
                            10.0 / 16.0, 1.5, south ? 1.0 : 10.0 / 16.0);
                        if (!west && !east && !north && !south)
                            shape.add(6.0 / 16.0, 0, 6.0 / 16.0, 10.0 / 16.0, 1.5, 10.0 / 16.0);
                    }
                    break;
                }
                case CollisionKind::Pane: {
                    const auto connects = [&](const int nx, const int nz) {
                        return BlockUtil::canPaneConnectTo(getBlockAt(nx, y, nz));
                    };
                    const bool west = connects(x - 1, z);
                    const bool east = connects(x + 1, z);
                    const bool north = connects(x, z - 1);
                    const bool south = connects(x, z + 1);
                    if (!west && !east && !north && !south) {
                        shape.add(0, 0, 7.0 / 16.0, 1, 1, 9.0 / 16.0);
                        shape.add(7.0 / 16.0, 0, 0, 9.0 / 16.0, 1, 1);
                        break;
                    }
                    shape.add(7.0 / 16.0, 0, 7.0 / 16.0, 9.0 / 16.0, 1, 9.0 / 16.0);
                    if (west) shape.add(0, 0, 7.0 / 16.0, 7.0 / 16.0, 1, 9.0 / 16.0);
                    if (east) shape.add(9.0 / 16.0, 0, 7.0 / 16.0, 1, 1, 9.0 / 16.0);
                    if (north) shape.add(7.0 / 16.0, 0, 0, 9.0 / 16.0, 1, 7.0 / 16.0);
                    if (south) shape.add(7.0 / 16.0, 0, 9.0 / 16.0, 9.0 / 16.0, 1, 1);
                    break;
                }
                case CollisionKind::Cauldron:
                    shape.add(0, 0, 0, 1, 5.0 / 16.0, 1);
                    shape.add(0, 0, 0, 2.0 / 16.0, 1, 1);
                    shape.add(14.0 / 16.0, 0, 0, 1, 1, 1);
                    shape.add(2.0 / 16.0, 0, 0, 14.0 / 16.0, 1, 2.0 / 16.0);
                    shape.add(2.0 / 16.0, 0, 14.0 / 16.0, 14.0 / 16.0, 1, 1);
                    break;
                case CollisionKind::FlowerPot:
                    shape.add(5.0 / 16.0, 0, 5.0 / 16.0, 11.0 / 16.0, 6.0 / 16.0, 11.0 / 16.0);
                    break;
                case CollisionKind::Skull:
                    switch (metadata & 7) {
                        case 2: shape.add(0.25, 0.25, 0.5, 0.75, 0.75, 1.0); break;
                        case 3: shape.add(0.25, 0.25, 0.0, 0.75, 0.75, 0.5); break;
                        case 4: shape.add(0.5, 0.25, 0.25, 1.0, 0.75, 0.75); break;
                        case 5: shape.add(0.0, 0.25, 0.25, 0.5, 0.75, 0.75); break;
                        default: shape.add(0.25, 0.0, 0.25, 0.75, 0.5, 0.75); break;
                    }
                    break;
                case CollisionKind::EndPortalFrame:
                    shape.add(0, 0, 0, 1, 13.0 / 16.0, 1);
                    break;
                case CollisionKind::DaylightDetector:
                    shape.add(0, 0, 0, 1, 6.0 / 16.0, 1);
                    break;
                case CollisionKind::Anvil:
                    if ((metadata & 1) == 0) shape.add(2.0 / 16.0, 0, 0, 14.0 / 16.0, 1, 1);
                    else shape.add(0, 0, 2.0 / 16.0, 1, 1, 14.0 / 16.0);
                    break;
                case CollisionKind::Ladder:
                    switch (metadata) {
                        case 2: shape.add(0, 0, 14.0 / 16.0, 1, 1, 1); break;
                        case 3: shape.add(0, 0, 0, 1, 1, 2.0 / 16.0); break;
                        case 4: shape.add(14.0 / 16.0, 0, 0, 1, 1, 1); break;
                        case 5: shape.add(0, 0, 0, 2.0 / 16.0, 1, 1); break;
                        default: break;
                    }
                    break;
                case CollisionKind::PistonHead:
                    switch (metadata & 7) {
                        case 0:
                            shape.add(0, 0, 0, 1, 0.25, 1);
                            shape.add(0.375, 0.25, 0.375, 0.625, 1, 0.625);
                            break;
                        case 1:
                            shape.add(0, 0.75, 0, 1, 1, 1);
                            shape.add(0.375, 0, 0.375, 0.625, 0.75, 0.625);
                            break;
                        case 2:
                            shape.add(0, 0, 0, 1, 1, 0.25);
                            shape.add(0.25, 0.375, 0.25, 0.75, 0.625, 1);
                            break;
                        case 3:
                            shape.add(0, 0, 0.75, 1, 1, 1);
                            shape.add(0.25, 0.375, 0, 0.75, 0.625, 0.75);
                            break;
                        case 4:
                            shape.add(0, 0, 0, 0.25, 1, 1);
                            shape.add(0.375, 0.25, 0.25, 0.625, 0.75, 1);
                            break;
                        case 5:
                            shape.add(0.75, 0, 0, 1, 1, 1);
                            shape.add(0, 0.375, 0.25, 0.75, 0.625, 0.75);
                            break;
                        default: break;
                    }
                    break;
                case CollisionKind::Full:
                    shape.add(0, 0, 0, 1, 1, 1);
                    break;
                }

                for (uint8_t index = 0; index < shape.count; ++index) {
                    if (const AABB worldBounds = shape.boxes[index].offset(x, y, z); worldBounds.intersects(box))
                        collisions.emplace_back(block, x, y, z, shape.boxes[index]);
                }
            }
        }
    }

    return collisions;
}
