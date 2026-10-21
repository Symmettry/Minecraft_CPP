#pragma once

#include <limits>

#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/net/packets/play/client/C07PacketPlayerDigging.hpp"
#include "net/lily/minecpp/net/packets/play/client/C08PacketPlayerBlockPlacement.hpp"
#include "net/lily/minecpp/net/packets/play/client/C0APacketAnimation.hpp"
#include "net/lily/minecpp/net/packets/play/client/C02PacketUseEntity.hpp"
#include "net/lily/minecpp/net/packets/play/client/C09PacketHeldItemChange.hpp"
#include "net/lily/minecpp/net/packets/play/client/C10PacketCreativeInventoryAction.hpp"
#include "net/lily/minecpp/util/ItemRegistry.hpp"

class PlayerControllerMP final {
public:
    explicit PlayerControllerMP(const Minecraft* mc) : mc(mc) {}

    void clickBlock(const glm::ivec3& position, const uint8_t facing) {
        if (mc->player->isSpectator()) return;
        if (isHittingBlock && position != currentBlock) abortDestroyBlock();

        const Block block = mc->world->getBlockAt(position.x, position.y, position.z);
        switch (blockId(block)) {
            case blockId(BLOCK_AIR):
            case blockId(BLOCK_WATER): case blockId(BLOCK_FLOWING_WATER):
            case blockId(BLOCK_LAVA): case blockId(BLOCK_FLOWING_LAVA):
                return;
            default:
                break;
        }

        mc->renderer->addBlockHitParticle(position, facing, block);
        send(C07PacketPlayerDigging::Action::StartDestroyBlock, position, facing);
        const NetworkItemStack& held = heldItem();
        miningItemId = held.itemId;
        miningItemDamage = held.damage;
        if (mc->player->gameType == GameType::CREATIVE || relativeHardness(block) >= 1.0f) {
            destroyBlock(position);
            if (mc->player->gameType == GameType::CREATIVE) blockHitDelay = 5;
            return;
        }

        isHittingBlock = true;
        currentBlock = position;
        currentFacing = facing;
        currentDamage = 0.0f;
        setBreakProgress(-1);
    }

    void damageBlock(const glm::ivec3& position, const uint8_t facing) {
        if (blockHitDelay > 0) {
            --blockHitDelay;
            return;
        }
        if (isHittingBlock && !isMiningWithSelectedItem()) {
            abortDestroyBlock();
        }
        if (!isHittingBlock || position != currentBlock) {
            clickBlock(position, facing);
            return;
        }

        const Block block = mc->world->getBlockAt(position.x, position.y, position.z);
        if (blockId(block) == blockId(BLOCK_AIR)) {
            clearBreakingState();
            return;
        }

        mc->renderer->addBlockHitParticle(position, facing, block);

        currentDamage += relativeHardness(block);
        if (currentDamage >= 1.0f) {
            send(C07PacketPlayerDigging::Action::StopDestroyBlock, position, facing);
            destroyBlock(position);
            clearBreakingState();
            blockHitDelay = 5;
            return;
        }
        setBreakProgress(static_cast<int>(currentDamage * 10.0f) - 1);
    }

    void abortDestroyBlock() {
        if (!isHittingBlock) return;
        send(C07PacketPlayerDigging::Action::AbortDestroyBlock, currentBlock, 0);
        clearBreakingState();
    }

    void heldItemChanged() {
        abortDestroyBlock();
        syncCurrentPlayItem();
    }

    void syncCurrentPlayItem() const {
        const int slot = mc->player->currentHotbarSlot;
        if (slot == serverHotbarSlot || mc->netClient == nullptr) return;
        serverHotbarSlot = slot;
        mc->netClient->sendPacket(C09PacketHeldItemChange{slot});
    }

    [[nodiscard]] bool getIsHittingBlock() const {
        return isHittingBlock;
    }

    // minecraft.middleClickMouse: in creative, copy the looked-at block into the hotbar
    void pickBlock(const MovingObjectPosition& hit) const {
        if (mc->player->gameType != GameType::CREATIVE || mc->netClient == nullptr) return;
        if (!hit.valid || hit.typeOfHit != MovingObjectPosition::BLOCK) return;
        const Block block = mc->world->getBlockAt(hit.blockPos.x, hit.blockPos.y, hit.blockPos.z);
        const NetworkItemStack picked = pickedItem(block);
        if (!picked) return;

        auto& inventory = mc->player->inventory;
        // inventoryPlayer.setCurrentItem: reuse a hotbar slot already holding it, else an empty one, else replace
        int slot = -1;
        for (int hotbar = 0; hotbar < 9 && slot < 0; ++hotbar) {
            const NetworkItemStack& candidate = inventory[36 + hotbar];
            if (candidate && candidate.itemId == picked.itemId && candidate.damage == picked.damage) slot = hotbar;
        }
        if (slot >= 0) {
            mc->player->currentHotbarSlot = slot;
            syncCurrentPlayItem();
            return;
        }
        slot = mc->player->currentHotbarSlot;
        if (inventory[36 + slot]) {
            for (int hotbar = 0; hotbar < 9; ++hotbar) {
                if (!inventory[36 + hotbar]) { slot = hotbar; break; }
            }
        }
        mc->player->currentHotbarSlot = slot;
        inventory[36 + slot] = picked;
        syncCurrentPlayItem();
        mc->netClient->sendPacket(C10PacketCreativeInventoryAction{static_cast<int16_t>(36 + slot), picked});
    }

    void attackEntity(const Entity& target) const {
        syncCurrentPlayItem();
        if (mc->netClient != nullptr) mc->netClient->sendPacket(
            C02PacketUseEntity{target.entityId, C02PacketUseEntity::Action::Attack});
        // entityPlayer.attackTargetEntityWithCurrentItem: a sprint hit contributes one level of knockback, then slows the attacker and ends their sprint locally
        if (mc->player->isSprinting()) {
            mc->player->velocity.x *= 0.6;
            mc->player->velocity.z *= 0.6;
            mc->player->setSprinting(false);
        }
        // entityPlayer.attackTargetEntityWithCurrentItem: a hit while falling is a critical hit, and the attacker's client shows the particles itself without waiting for the server
        const EntityPlayer& player = *mc->player;
        const bool living = target.getType() == MOB || target.getType() == PLAYER;
        if (living && player.fallDistance > 0.0f && !player.onGround && !player.isOnLadder()
            && !player.isInWater() && player.ridingEntity == nullptr && !player.isSpectator())
            mc->renderer->emitParticlesAtEntity(target.entityId, 9);
    }

    // playerControllerMP.isPlayerRightClickingOnEntity + interactWithEntitySendPacket
    void interactEntity(const Entity& target, const glm::dvec3& hitVec) const {
        if (mc->netClient == nullptr) return;
        syncCurrentPlayItem();
        const glm::vec3 offset = glm::vec3(hitVec - target.position);
        mc->netClient->sendPacket(C02PacketUseEntity{target.entityId, C02PacketUseEntity::Action::InteractAt, offset});
        mc->netClient->sendPacket(C02PacketUseEntity{target.entityId, C02PacketUseEntity::Action::Interact});
    }

    void rightClick(const MovingObjectPosition& hit) const {
        if (mc->player->isSpectator()) return;
        syncCurrentPlayItem();
        bool swing = false;
        if (hit.valid && hit.typeOfHit == MovingObjectPosition::ENTITY && hit.entityHit != nullptr) {
            interactEntity(*hit.entityHit, hit.hitVec);
            swing = true;
        }
        NetworkItemStack& held = mc->player->inventory[36 + mc->player->currentHotbarSlot];
        if (hit.valid && hit.typeOfHit == MovingObjectPosition::BLOCK) {
            // playerControllerMP.onPlayerRightClick uses activatable blocks unless the player sneaks with an item
            const Block clicked = mc->world->getBlockAt(hit.blockPos.x, hit.blockPos.y, hit.blockPos.z);
            if (BlockUtil::isActivatable(clicked) && !(mc->player->isSneaking() && held)) {
                const glm::vec3 offset = glm::vec3(hit.hitVec - glm::dvec3(hit.blockPos));
                predictActivation(hit.blockPos, clicked);
                if (mc->netClient != nullptr) {
                    mc->netClient->sendPacket(C08PacketPlayerBlockPlacement{hit.blockPos, hit.sideHit, held, offset});
                    mc->netClient->sendPacket(C0APacketAnimation{});
                }
                mc->player->swingItem();
                return;
            }
            switch (held.itemId) {
                case 326: case 327: {
                    const NetworkItemStack packetHeld = held;
                    const glm::vec3 offset = glm::vec3(hit.hitVec - glm::dvec3(hit.blockPos));
                    if (mc->netClient != nullptr) {
                        mc->netClient->sendPacket(C08PacketPlayerBlockPlacement{
                            hit.blockPos, hit.sideHit, packetHeld, offset});
                        mc->netClient->sendPacket(C08PacketPlayerBlockPlacement{
                            {-1, -1, -1}, 255, packetHeld, {0.0f, 0.0f, 0.0f}});
                    }
                    predictFilledBucket(hit, held);
                    return;
                }
                default: break;
            }
            const glm::vec3 hitPosition = glm::vec3(hit.hitVec - glm::dvec3(hit.blockPos));
            const bool blockItem = held && held.itemId > 0 && held.itemId < 256;
            const NetworkItemStack packetHeld = held;
            if (blockItem && !predictPlacement(hit, held)) return;
            if (mc->netClient != nullptr) mc->netClient->sendPacket(C08PacketPlayerBlockPlacement{
                hit.blockPos, hit.sideHit, packetHeld, hitPosition});
            swing = true;
            if (!blockItem && held && mc->netClient != nullptr)
                mc->netClient->sendPacket(C08PacketPlayerBlockPlacement{
                    {-1, -1, -1}, 255, held, {0.0f, 0.0f, 0.0f}});
        } else if (held && mc->netClient != nullptr) {
            mc->netClient->sendPacket(C08PacketPlayerBlockPlacement{
                {-1, -1, -1}, 255, held, {0.0f, 0.0f, 0.0f}});
        }
        if (!swing) return;
        mc->player->swingItem();
        if (mc->netClient != nullptr) mc->netClient->sendPacket(C0APacketAnimation{});
    }

    void stopUsingItem() const {
        if (mc->netClient != nullptr) mc->netClient->sendPacket(C07PacketPlayerDigging{
            C07PacketPlayerDigging::Action::ReleaseUseItem, {0, 0, 0}, 0});
    }

private:
    enum class ToolType : uint8_t { NONE, PICKAXE, AXE, SHOVEL, SWORD, SHEARS };

    struct ToolProperties {
        ToolType type = ToolType::NONE;
        float efficiency = 1.0f;
        int harvestLevel = -1;
    };

    static constexpr int32_t LOCAL_BREAKER_ID = std::numeric_limits<int32_t>::min();

    const Minecraft* mc;
    glm::ivec3 currentBlock{-1, -1, -1};
    uint8_t currentFacing = 0;
    float currentDamage = 0.0f;
    int blockHitDelay = 0;
    bool isHittingBlock = false;
    int16_t miningItemId = -1;
    int16_t miningItemDamage = 0;
    mutable int serverHotbarSlot = -1;

    [[nodiscard]] const NetworkItemStack& heldItem() const {
        return mc->player->inventory[36 + mc->player->currentHotbarSlot];
    }

    [[nodiscard]] bool isMiningWithSelectedItem() const {
        const NetworkItemStack& held = heldItem();
        return held.itemId == miningItemId && held.damage == miningItemDamage;
    }

    void predictActivation(const glm::ivec3& position, const Block block) const {
        const auto updateMetadata = [&](const glm::ivec3& target, const Block current, const uint8_t metadata) {
            mc->world->setBlockAt(target.x, target.y, target.z,
                static_cast<Block>((blockId(current) << 4) | (metadata & 15)));
        };
        switch (blockId(block)) {
            case blockId(BLOCK_WOODEN_DOOR): case blockId(BLOCK_SPRUCE_DOOR):
            case blockId(BLOCK_BIRCH_DOOR): case blockId(BLOCK_JUNGLE_DOOR):
            case blockId(BLOCK_ACACIA_DOOR): case blockId(BLOCK_DARK_OAK_DOOR): {
                const bool upper = (blockMeta(block) & 8) != 0;
                const glm::ivec3 lowerPosition = upper ? position - glm::ivec3(0, 1, 0) : position;
                const Block lower = mc->world->getBlockAt(lowerPosition.x, lowerPosition.y, lowerPosition.z);
                if (blockId(lower) == blockId(block)) updateMetadata(lowerPosition, lower, blockMeta(lower) ^ 4);
                break;
            }
            case blockId(BLOCK_TRAPDOOR):
            case blockId(BLOCK_FENCE_GATE): case blockId(BLOCK_SPRUCE_FENCE_GATE):
            case blockId(BLOCK_BIRCH_FENCE_GATE): case blockId(BLOCK_JUNGLE_FENCE_GATE):
            case blockId(BLOCK_DARK_OAK_FENCE_GATE): case blockId(BLOCK_ACACIA_FENCE_GATE):
                updateMetadata(position, block, blockMeta(block) ^ 4);
                break;
            case blockId(BLOCK_STONE_BUTTON): case blockId(BLOCK_WOODEN_BUTTON):
            case blockId(BLOCK_LEVER):
                updateMetadata(position, block, blockMeta(block) ^ 8);
                break;
            case blockId(BLOCK_UNPOWERED_REPEATER): case blockId(BLOCK_POWERED_REPEATER):
                updateMetadata(position, block, static_cast<uint8_t>((blockMeta(block) & 3)
                    | ((blockMeta(block) + 4) & 12)));
                break;
            case blockId(BLOCK_UNPOWERED_COMPARATOR): case blockId(BLOCK_POWERED_COMPARATOR):
                updateMetadata(position, block, blockMeta(block) ^ 4);
                break;
            default:
                break;
        }
    }

    void predictFilledBucket(const MovingObjectPosition& hit, NetworkItemStack& held) const {
        glm::ivec3 target = hit.blockPos;
        switch (hit.sideHit) {
            case 0: --target.y; break;
            case 1: ++target.y; break;
            case 2: --target.z; break;
            case 3: ++target.z; break;
            case 4: --target.x; break;
            case 5: ++target.x; break;
            default: return;
        }
        if (target.y < 0 || target.y >= WORLD_HEIGHT) return;
        const Block current = mc->world->getBlockAt(target.x, target.y, target.z);
        switch (blockId(current)) {
            case blockId(BLOCK_AIR): case blockId(BLOCK_WATER): case blockId(BLOCK_FLOWING_WATER):
            case blockId(BLOCK_LAVA): case blockId(BLOCK_FLOWING_LAVA): case blockId(BLOCK_FIRE):
            case blockId(BLOCK_TALLGRASS): case blockId(BLOCK_DEADBUSH): case blockId(BLOCK_VINE):
            case blockId(BLOCK_DOUBLE_PLANT): break;
            default: return;
        }
        if (held.itemId == 326 && mc->player->dimension == -1) return;
        const Block liquid = held.itemId == 326 ? BLOCK_FLOWING_WATER : BLOCK_FLOWING_LAVA;
        mc->world->setBlockAt(target.x, target.y, target.z, liquid);
        if (mc->player->gameType != GameType::CREATIVE) held = {325, 1, 0};
    }

    bool predictPlacement(const MovingObjectPosition& hit, NetworkItemStack& held) const {
        if (!held || held.itemId <= 0 || held.itemId >= 256) return false;
        glm::ivec3 target = hit.blockPos;
        const Block existing = mc->world->getBlockAt(target.x, target.y, target.z);
        // itemSlab: a matching half slab clicked on its open face becomes a double slab
        if (blockId(existing) == held.itemId && (held.itemId == blockId(BLOCK_STONE_SLAB)
                || held.itemId == blockId(BLOCK_WOODEN_SLAB) || held.itemId == blockId(BLOCK_STONE_SLAB2))
            && (blockMeta(existing) & 7) == (held.damage & 7)
            && ((hit.sideHit == 1 && (blockMeta(existing) & 8) == 0)
                || (hit.sideHit == 0 && (blockMeta(existing) & 8) != 0))) {
            const uint16_t doubled = held.itemId == blockId(BLOCK_STONE_SLAB) ? blockId(BLOCK_DOUBLE_STONE_SLAB)
                : held.itemId == blockId(BLOCK_WOODEN_SLAB) ? blockId(BLOCK_DOUBLE_WOODEN_SLAB)
                : blockId(BLOCK_DOUBLE_STONE_SLAB2);
            mc->world->setBlockAt(target.x, target.y, target.z,
                static_cast<Block>((doubled << 4) | (held.damage & 7)));
            if (mc->player->gameType != GameType::CREATIVE && --held.count == 0) held = {};
            return true;
        }
        bool replaceable = false;
        switch (blockId(existing)) {
            case blockId(BLOCK_AIR): case blockId(BLOCK_TALLGRASS): case blockId(BLOCK_DEADBUSH):
            case blockId(BLOCK_SNOW_LAYER): case blockId(BLOCK_YELLOW_FLOWER):
            case blockId(BLOCK_RED_FLOWER): case blockId(BLOCK_BROWN_MUSHROOM):
            case blockId(BLOCK_RED_MUSHROOM):
                replaceable = true;
                break;
            default:
                switch (hit.sideHit) {
                    case 0: --target.y; break;
                    case 1: ++target.y; break;
                    case 2: --target.z; break;
                    case 3: ++target.z; break;
                    case 4: --target.x; break;
                    case 5: ++target.x; break;
                    default: return false;
                }
                break;
        }
        // block.isReplaceable: air, liquids, vines, fire and the like can be built over
        const auto canBuildOver = [](const Block block) {
            switch (blockId(block)) {
                case blockId(BLOCK_AIR): case blockId(BLOCK_WATER): case blockId(BLOCK_FLOWING_WATER):
                case blockId(BLOCK_LAVA): case blockId(BLOCK_FLOWING_LAVA): case blockId(BLOCK_VINE):
                case blockId(BLOCK_FIRE): case blockId(BLOCK_TALLGRASS): case blockId(BLOCK_DEADBUSH):
                case blockId(BLOCK_DOUBLE_PLANT):
                    return true;
                default:
                    return false;
            }
        };
        if (target.y < 0 || target.y >= WORLD_HEIGHT
            || (!replaceable && !canBuildOver(mc->world->getBlockAt(target.x, target.y, target.z)))) return false;
        const double hitY = hit.hitVec.y - static_cast<double>(hit.blockPos.y);
        if (!canPlaceBlockAt(held.itemId, target, hit)) return false;
        const uint8_t metadata = placementMetadata(held.itemId, held.damage, hit.sideHit, hitY);
        const Block placed = static_cast<Block>((static_cast<uint16_t>(held.itemId) << 4) | metadata);
        if (!canPlaceWithoutEntityCollision(placed, target)) return false;
        mc->world->setBlockAt(target.x, target.y, target.z, placed);
        if (mc->player->gameType != GameType::CREATIVE && --held.count == 0) held = {};
        return true;
    }

    // block.canPlaceBlockAt for the blocks that need support, so the client does not predict a placement the server will reject
    bool canPlaceBlockAt(const int16_t itemId, const glm::ivec3& target, const MovingObjectPosition& hit) const {
        const auto blockAt = [&](const int x, const int y, const int z) { return mc->world->getBlockAt(x, y, z); };
        const Block below = blockAt(target.x, target.y - 1, target.z);
        const auto solidTop = [](const Block block) {
            if (BlockUtil::isFullBlock(block)) return true;
            switch (blockId(block)) {
                case blockId(BLOCK_GLASS): case blockId(BLOCK_STAINED_GLASS): case blockId(BLOCK_LEAVES):
                case blockId(BLOCK_LEAVES2): case blockId(BLOCK_ICE): case blockId(BLOCK_PACKED_ICE):
                case blockId(BLOCK_SLIME): case blockId(BLOCK_FARMLAND): case blockId(BLOCK_SOUL_SAND):
                    return true;
                default: return false;
            }
        };
        switch (itemId) {
            case blockId(BLOCK_RAIL): case blockId(BLOCK_GOLDEN_RAIL):
            case blockId(BLOCK_DETECTOR_RAIL): case blockId(BLOCK_ACTIVATOR_RAIL):
                return solidTop(below);
            case blockId(BLOCK_SAPLING): case blockId(BLOCK_YELLOW_FLOWER): case blockId(BLOCK_RED_FLOWER):
            case blockId(BLOCK_DEADBUSH): case blockId(BLOCK_TALLGRASS):
                return solidTop(below);
            case blockId(BLOCK_WATERLILY): {
                const uint16_t id = blockId(below);
                return (id == blockId(BLOCK_WATER) || id == blockId(BLOCK_FLOWING_WATER)) && blockMeta(below) == 0;
            }
            case blockId(BLOCK_TORCH): case blockId(BLOCK_REDSTONE_TORCH): {
                // on top of a solid block, or on the side of one; never hanging from a ceiling
                if (hit.sideHit == 0) return false;
                const glm::ivec3 support = hit.blockPos;
                if (hit.sideHit == 1) return solidTop(blockAt(support.x, support.y, support.z))
                    || blockId(blockAt(support.x, support.y, support.z)) == blockId(BLOCK_FENCE);
                return BlockUtil::isFullBlock(blockAt(support.x, support.y, support.z));
            }
            case blockId(BLOCK_LADDER):
                return hit.sideHit >= 2 && BlockUtil::isFullBlock(blockAt(hit.blockPos.x, hit.blockPos.y, hit.blockPos.z));
            default:
                return true;
        }
    }

    bool canPlaceWithoutEntityCollision(const Block block, const glm::ivec3& position) const {
        if (!BlockUtil::blocksMovement(block)) return true;

        AABB bounds{static_cast<double>(position.x), static_cast<double>(position.y), static_cast<double>(position.z),
                    position.x + 1.0, position.y + 1.0, position.z + 1.0};
        switch (blockId(block)) {
            case blockId(BLOCK_STONE_SLAB): case blockId(BLOCK_WOODEN_SLAB):
            case blockId(BLOCK_STONE_SLAB2):
                if ((blockMeta(block) & 8) != 0) bounds.minY += 0.5;
                else bounds.maxY -= 0.5;
                break;
            case blockId(BLOCK_CARPET): bounds.maxY = bounds.minY + 1.0 / 16.0; break;
            case blockId(BLOCK_FARMLAND): bounds.maxY = bounds.minY + 15.0 / 16.0; break;
            case blockId(BLOCK_SOUL_SAND): bounds.maxY = bounds.minY + 14.0 / 16.0; break;
            default: break;
        }
        return !mc->world->hasEntityCollision(bounds);
    }

    uint8_t placementMetadata(const int16_t itemId, const int16_t damage,
                              const uint8_t facing, const double hitY) const {
        const uint8_t metadata = static_cast<uint8_t>(damage & 15);
        // slabs and stairs sit in the top half when clicked on a bottom face or on the upper half of a side
        const bool upperHalf = facing == 0 || (facing != 1 && hitY > 0.5);
        if (BlockUtil::isStair(static_cast<Block>(static_cast<uint16_t>(itemId) << 4))) {
            // blockStairs.onBlockPlaced: facing = the placer's horizontal facing, stored as 5 - index
            static constexpr uint8_t stairFacing[4]{2, 1, 3, 0}; // south, west, north, east
            return static_cast<uint8_t>(stairFacing[horizontalFacing()] | (upperHalf ? 4 : 0));
        }
        switch (itemId) {
            case blockId(BLOCK_TORCH): case blockId(BLOCK_REDSTONE_TORCH):
                // 5 stands on top; the sides lean away from the wall: clicked north face -> 4, south 3, west 2, east 1
                switch (facing) {
                    case 2: return 4;
                    case 3: return 3;
                    case 4: return 2;
                    case 5: return 1;
                    default: return 5;
                }
            case blockId(BLOCK_LADDER): return facing >= 2 ? facing : 2;
            case blockId(BLOCK_STONE_SLAB): case blockId(BLOCK_WOODEN_SLAB): case blockId(BLOCK_STONE_SLAB2):
                return static_cast<uint8_t>((metadata & 7) | (upperHalf ? 8 : 0));
            case blockId(BLOCK_LOG):
            case blockId(BLOCK_LOG2):
            case blockId(BLOCK_HAY_BLOCK):
                switch (facing) {
                    case 0: case 1: return metadata & 3;
                    case 2: case 3: return static_cast<uint8_t>((metadata & 3) | 8);
                    case 4: case 5: return static_cast<uint8_t>((metadata & 3) | 4);
                    default: return metadata;
                }
            case blockId(BLOCK_QUARTZ_BLOCK):
                if (metadata != 2) return metadata;
                switch (facing) {
                    case 0: case 1: return 2;
                    case 2: case 3: return 4;
                    case 4: case 5: return 3;
                    default: return metadata;
                }
            case blockId(BLOCK_FURNACE): case blockId(BLOCK_LIT_FURNACE):
            case blockId(BLOCK_CHEST): case blockId(BLOCK_TRAPPED_CHEST):
            case blockId(BLOCK_ENDER_CHEST):
                switch (horizontalFacing()) {
                    case 0: return 2;
                    case 1: return 5;
                    case 2: return 3;
                    case 3: return 4;
                    default: return metadata;
                }
            case blockId(BLOCK_PUMPKIN): case blockId(BLOCK_LIT_PUMPKIN):
                return static_cast<uint8_t>((horizontalFacing() + 2) & 3);
            default:
                return metadata;
        }
    }

    // block.getPickBlock: the item a block is picked as (meta reduced to the item's damage value)
    static NetworkItemStack pickedItem(const Block block) {
        const uint16_t id = blockId(block);
        const uint8_t meta = blockMeta(block);
        int16_t item = static_cast<int16_t>(id);
        int16_t damage = 0;
        switch (id) { // nothing to pick
            case blockId(BLOCK_AIR): case blockId(BLOCK_WATER): case blockId(BLOCK_FLOWING_WATER):
            case blockId(BLOCK_LAVA): case blockId(BLOCK_FLOWING_LAVA): case blockId(BLOCK_FIRE):
            case blockId(BLOCK_PORTAL): case blockId(BLOCK_END_PORTAL):
                return {};
            default: break;
        }
        switch (id) {
            case blockId(BLOCK_LOG): case blockId(BLOCK_LOG2): case blockId(BLOCK_LEAVES): case blockId(BLOCK_LEAVES2):
            case blockId(BLOCK_SAPLING): damage = meta & 3; if (id == blockId(BLOCK_SAPLING)) damage = meta & 7; break;
            case blockId(BLOCK_STONE_SLAB): case blockId(BLOCK_WOODEN_SLAB): case blockId(BLOCK_STONE_SLAB2):
                damage = meta & 7; break;
            case blockId(BLOCK_DOUBLE_STONE_SLAB): item = blockId(BLOCK_STONE_SLAB); damage = meta & 7; break;
            case blockId(BLOCK_DOUBLE_WOODEN_SLAB): item = blockId(BLOCK_WOODEN_SLAB); damage = meta & 7; break;
            case blockId(BLOCK_DOUBLE_STONE_SLAB2): item = blockId(BLOCK_STONE_SLAB2); damage = meta & 7; break;
            case blockId(BLOCK_LIT_FURNACE): item = blockId(BLOCK_FURNACE); break;
            case blockId(BLOCK_LIT_REDSTONE_ORE): item = blockId(BLOCK_REDSTONE_ORE); break;
            case blockId(BLOCK_FARMLAND): item = blockId(BLOCK_DIRT); break;
            case blockId(BLOCK_WOOL): case blockId(BLOCK_STAINED_HARDENED_CLAY): case blockId(BLOCK_STAINED_GLASS):
            case blockId(BLOCK_STAINED_GLASS_PANE): case blockId(BLOCK_CARPET): case blockId(BLOCK_PLANKS):
            case blockId(BLOCK_STONE): case blockId(BLOCK_SAND): case blockId(BLOCK_SANDSTONE):
            case blockId(BLOCK_STONEBRICK): case blockId(BLOCK_QUARTZ_BLOCK): case blockId(BLOCK_RED_SANDSTONE):
            case blockId(BLOCK_PRISMARINE): case blockId(BLOCK_DIRT): case blockId(BLOCK_SPONGE):
            case blockId(BLOCK_COBBLESTONE_WALL): case blockId(BLOCK_MONSTER_EGG):
            case blockId(BLOCK_RED_FLOWER): case blockId(BLOCK_YELLOW_FLOWER): case blockId(BLOCK_DOUBLE_PLANT):
                damage = meta; if (id == blockId(BLOCK_DOUBLE_PLANT)) damage = meta & 7; break;
            case blockId(BLOCK_TALLGRASS): damage = meta; break;
            case blockId(BLOCK_QUARTZ_STAIRS): break;
            case blockId(BLOCK_WHEAT): item = 295; break;           // seeds
            case blockId(BLOCK_CARROTS): item = 391; break;
            case blockId(BLOCK_POTATOES): item = 392; break;
            case blockId(BLOCK_PUMPKIN_STEM): item = 361; break;
            case blockId(BLOCK_MELON_STEM): item = 362; break;
            case blockId(BLOCK_REEDS): item = 338; break;
            case blockId(BLOCK_NETHER_WART): item = 372; break;
            case blockId(BLOCK_REDSTONE_WIRE): item = 331; break;
            case blockId(BLOCK_STANDING_SIGN): case blockId(BLOCK_WALL_SIGN): item = 323; break;
            case blockId(BLOCK_WOODEN_DOOR): item = 324; break;
            case blockId(BLOCK_IRON_DOOR): item = 330; break;
            case blockId(BLOCK_SPRUCE_DOOR): item = 427; break;
            case blockId(BLOCK_BIRCH_DOOR): item = 428; break;
            case blockId(BLOCK_JUNGLE_DOOR): item = 429; break;
            case blockId(BLOCK_ACACIA_DOOR): item = 430; break;
            case blockId(BLOCK_DARK_OAK_DOOR): item = 431; break;
            case blockId(BLOCK_BED): item = 355; break;
            case blockId(BLOCK_CAKE): item = 354; break;
            case blockId(BLOCK_UNPOWERED_REPEATER): case blockId(BLOCK_POWERED_REPEATER): item = 356; break;
            case blockId(BLOCK_UNPOWERED_COMPARATOR): case blockId(BLOCK_POWERED_COMPARATOR): item = 404; break;
            case blockId(BLOCK_FLOWER_POT): item = 390; break;
            case blockId(BLOCK_CAULDRON): item = 380; break;
            case blockId(BLOCK_BREWING_STAND): item = 379; break;
            case blockId(BLOCK_TRIPWIRE): item = 287; break;
            case blockId(BLOCK_COCOA): item = 351; damage = 3; break;
            case blockId(BLOCK_SKULL): item = 397; break;
            case blockId(BLOCK_UNLIT_REDSTONE_TORCH): item = blockId(BLOCK_REDSTONE_TORCH); break;
            case blockId(BLOCK_STANDING_BANNER): case blockId(BLOCK_WALL_BANNER): item = 425; break;
            default: break;
        }
        NetworkItemStack stack;
        stack.itemId = item;
        stack.count = 1;
        stack.damage = damage;
        return stack;
    }

    [[nodiscard]] int horizontalFacing() const {
        return static_cast<int>(std::floor(mc->player->rotation.yaw * 4.0f / 360.0f + 0.5f)) & 3;
    }

    void send(const C07PacketPlayerDigging::Action action, const glm::ivec3& position,
              const uint8_t facing) const {
        if (mc->netClient != nullptr) mc->netClient->sendPacket(C07PacketPlayerDigging{action, position, facing});
    }

    void destroyBlock(const glm::ivec3& position) const {
        const Block block = mc->world->getBlockAt(position.x, position.y, position.z);
        mc->renderer->addBlockDestroyParticles(position, block);
        mc->world->setBlockAt(position.x, position.y, position.z, BLOCK_AIR);
    }

    void setBreakProgress(const int progress) const {
        std::lock_guard lock(mc->world->blockEventMutex);
        if (progress < 0 || progress > 9) mc->world->damagedBlocks.erase(LOCAL_BREAKER_ID);
        else mc->world->damagedBlocks[LOCAL_BREAKER_ID] = {currentBlock, static_cast<uint8_t>(progress)};
    }

    void clearBreakingState() {
        setBreakProgress(-1);
        isHittingBlock = false;
        currentDamage = 0.0f;
        currentBlock = {-1, -1, -1};
        miningItemId = -1;
        miningItemDamage = 0;
    }

    static float hardness(const Block block) {
        switch (blockId(block)) {
            case blockId(BLOCK_BEDROCK): case blockId(BLOCK_WATER): case blockId(BLOCK_FLOWING_WATER):
            case blockId(BLOCK_LAVA): case blockId(BLOCK_FLOWING_LAVA): return -1.0f;
            case blockId(BLOCK_TALLGRASS): case blockId(BLOCK_DEADBUSH): case blockId(BLOCK_SLIME):
            case blockId(BLOCK_YELLOW_FLOWER): case blockId(BLOCK_RED_FLOWER):
            case blockId(BLOCK_BROWN_MUSHROOM): case blockId(BLOCK_RED_MUSHROOM):
            case blockId(BLOCK_FLOWER_POT): case blockId(BLOCK_UNPOWERED_REPEATER):
            case blockId(BLOCK_POWERED_REPEATER): case blockId(BLOCK_UNPOWERED_COMPARATOR):
            case blockId(BLOCK_POWERED_COMPARATOR): case blockId(BLOCK_PUMPKIN_STEM):
            case blockId(BLOCK_MELON_STEM): case blockId(BLOCK_DOUBLE_PLANT): return 0.0f;
            case blockId(BLOCK_SNOW_LAYER): case blockId(BLOCK_CARPET): return 0.1f;
            case blockId(BLOCK_SNOW): case blockId(BLOCK_LEAVES): case blockId(BLOCK_LEAVES2):
            case blockId(BLOCK_BROWN_MUSHROOM_BLOCK): case blockId(BLOCK_RED_MUSHROOM_BLOCK):
            case blockId(BLOCK_VINE): case blockId(BLOCK_COCOA): return 0.2f;
            case blockId(BLOCK_GLASS): case blockId(BLOCK_GLASS_PANE): case blockId(BLOCK_STAINED_GLASS):
            case blockId(BLOCK_STAINED_GLASS_PANE): case blockId(BLOCK_GLOWSTONE):
            case blockId(BLOCK_SEA_LANTERN): case blockId(BLOCK_REDSTONE_LAMP):
            case blockId(BLOCK_LIT_REDSTONE_LAMP): return 0.3f;
            case blockId(BLOCK_NETHERRACK): case blockId(BLOCK_LADDER): case blockId(BLOCK_CACTUS): return 0.4f;
            case blockId(BLOCK_DIRT): case blockId(BLOCK_SAND): case blockId(BLOCK_SOUL_SAND):
            case blockId(BLOCK_ICE): case blockId(BLOCK_PACKED_ICE): case blockId(BLOCK_HAY_BLOCK):
            case blockId(BLOCK_CAKE): case blockId(BLOCK_LEVER): case blockId(BLOCK_STONE_BUTTON):
            case blockId(BLOCK_WOODEN_BUTTON): case blockId(BLOCK_BREWING_STAND): return 0.5f;
            case blockId(BLOCK_GRASS): case blockId(BLOCK_GRAVEL): case blockId(BLOCK_CLAY):
            case blockId(BLOCK_FARMLAND): case blockId(BLOCK_MYCELIUM): return 0.6f;
            case blockId(BLOCK_RAIL): case blockId(BLOCK_GOLDEN_RAIL): case blockId(BLOCK_DETECTOR_RAIL):
            case blockId(BLOCK_ACTIVATOR_RAIL): return 0.7f;
            case blockId(BLOCK_MONSTER_EGG): return 0.75f;
            case blockId(BLOCK_WOOL): case blockId(BLOCK_QUARTZ_BLOCK): case blockId(BLOCK_QUARTZ_STAIRS):
            case blockId(BLOCK_SANDSTONE): case blockId(BLOCK_SANDSTONE_STAIRS):
            case blockId(BLOCK_RED_SANDSTONE): case blockId(BLOCK_RED_SANDSTONE_STAIRS): return 0.8f;
            case blockId(BLOCK_PUMPKIN): case blockId(BLOCK_LIT_PUMPKIN): case blockId(BLOCK_MELON_BLOCK):
            case blockId(BLOCK_SKULL): case blockId(BLOCK_STANDING_SIGN): case blockId(BLOCK_WALL_SIGN):
            case blockId(BLOCK_STANDING_BANNER): case blockId(BLOCK_WALL_BANNER): return 1.0f;
            case blockId(BLOCK_HARDENED_CLAY): case blockId(BLOCK_STAINED_HARDENED_CLAY): return 1.25f;
            case blockId(BLOCK_STONE): case blockId(BLOCK_BOOKSHELF): case blockId(BLOCK_STONEBRICK):
            case blockId(BLOCK_STONE_BRICK_STAIRS): case blockId(BLOCK_PRISMARINE): return 1.5f;
            case blockId(BLOCK_COBBLESTONE): case blockId(BLOCK_PLANKS): case blockId(BLOCK_LOG):
            case blockId(BLOCK_LOG2): case blockId(BLOCK_DOUBLE_WOODEN_SLAB):
            case blockId(BLOCK_WOODEN_SLAB): case blockId(BLOCK_OAK_STAIRS):
            case blockId(BLOCK_SPRUCE_STAIRS): case blockId(BLOCK_BIRCH_STAIRS):
            case blockId(BLOCK_JUNGLE_STAIRS): case blockId(BLOCK_ACACIA_STAIRS):
            case blockId(BLOCK_DARK_OAK_STAIRS): case blockId(BLOCK_STONE_STAIRS):
            case blockId(BLOCK_BRICK_BLOCK): case blockId(BLOCK_BRICK_STAIRS):
            case blockId(BLOCK_NETHER_BRICK): case blockId(BLOCK_NETHER_BRICK_STAIRS):
            case blockId(BLOCK_DOUBLE_STONE_SLAB): case blockId(BLOCK_STONE_SLAB):
            case blockId(BLOCK_DOUBLE_STONE_SLAB2): case blockId(BLOCK_STONE_SLAB2): return 2.0f;
            case blockId(BLOCK_COAL_ORE): case blockId(BLOCK_IRON_ORE): case blockId(BLOCK_GOLD_ORE):
            case blockId(BLOCK_LAPIS_ORE): case blockId(BLOCK_REDSTONE_ORE):
            case blockId(BLOCK_LIT_REDSTONE_ORE): case blockId(BLOCK_DIAMOND_ORE):
            case blockId(BLOCK_EMERALD_ORE): return 3.0f;
            case blockId(BLOCK_OBSIDIAN): return 50.0f;
            case blockId(BLOCK_END_PORTAL_FRAME): case blockId(BLOCK_END_PORTAL): case blockId(BLOCK_PORTAL):
            case blockId(BLOCK_COMMAND_BLOCK): case blockId(BLOCK_BARRIER): return -1.0f;
            case blockId(BLOCK_BED): case blockId(BLOCK_DAYLIGHT_DETECTOR):
            case blockId(BLOCK_DAYLIGHT_DETECTOR_INVERTED): return 0.2f;
            case blockId(BLOCK_COBBLESTONE_WALL): case blockId(BLOCK_FENCE): case blockId(BLOCK_NETHER_BRICK_FENCE):
            case blockId(BLOCK_SPRUCE_FENCE): case blockId(BLOCK_BIRCH_FENCE): case blockId(BLOCK_JUNGLE_FENCE):
            case blockId(BLOCK_DARK_OAK_FENCE): case blockId(BLOCK_ACACIA_FENCE): case blockId(BLOCK_FENCE_GATE):
            case blockId(BLOCK_SPRUCE_FENCE_GATE): case blockId(BLOCK_BIRCH_FENCE_GATE):
            case blockId(BLOCK_JUNGLE_FENCE_GATE): case blockId(BLOCK_DARK_OAK_FENCE_GATE):
            case blockId(BLOCK_ACACIA_FENCE_GATE):
                return 2.0f;
            case blockId(BLOCK_TRAPDOOR): case blockId(BLOCK_WOODEN_DOOR): case blockId(BLOCK_SPRUCE_DOOR):
            case blockId(BLOCK_BIRCH_DOOR): case blockId(BLOCK_JUNGLE_DOOR): case blockId(BLOCK_ACACIA_DOOR):
            case blockId(BLOCK_DARK_OAK_DOOR): case blockId(BLOCK_END_STONE): case blockId(BLOCK_DRAGON_EGG):
            case blockId(BLOCK_HOPPER): return 3.0f;
            case blockId(BLOCK_IRON_TRAPDOOR): case blockId(BLOCK_IRON_DOOR): case blockId(BLOCK_IRON_BARS):
            case blockId(BLOCK_ENCHANTING_TABLE): case blockId(BLOCK_ANVIL): case blockId(BLOCK_GOLD_BLOCK):
            case blockId(BLOCK_IRON_BLOCK): case blockId(BLOCK_LAPIS_BLOCK): case blockId(BLOCK_DIAMOND_BLOCK):
            case blockId(BLOCK_EMERALD_BLOCK): case blockId(BLOCK_REDSTONE_BLOCK):
            case blockId(BLOCK_COAL_BLOCK): return 5.0f;
            case blockId(BLOCK_WEB): return 4.0f;
            case blockId(BLOCK_FURNACE): case blockId(BLOCK_LIT_FURNACE): case blockId(BLOCK_DISPENSER):
            case blockId(BLOCK_DROPPER): return 3.5f;
            case blockId(BLOCK_CHEST): case blockId(BLOCK_TRAPPED_CHEST): case blockId(BLOCK_CRAFTING_TABLE):
                return 2.5f;
            case blockId(BLOCK_ENDER_CHEST): return 22.5f;
            case blockId(BLOCK_STONE_PRESSURE_PLATE): case blockId(BLOCK_WOODEN_PRESSURE_PLATE):
            case blockId(BLOCK_LIGHT_WEIGHTED_PRESSURE_PLATE): case blockId(BLOCK_HEAVY_WEIGHTED_PRESSURE_PLATE):
                return 0.5f;
            case blockId(BLOCK_TORCH): case blockId(BLOCK_REDSTONE_TORCH): case blockId(BLOCK_UNLIT_REDSTONE_TORCH):
            case blockId(BLOCK_SAPLING): case blockId(BLOCK_REEDS): case blockId(BLOCK_WHEAT):
            case blockId(BLOCK_CARROTS): case blockId(BLOCK_POTATOES): case blockId(BLOCK_NETHER_WART):
            case blockId(BLOCK_WATERLILY): case blockId(BLOCK_REDSTONE_WIRE): case blockId(BLOCK_TRIPWIRE):
                return 0.0f;
            default: return 1.0f;
        }
    }

    static ToolProperties getToolProperties(const int16_t itemId) {
        switch (itemId) {
            case 256: return {ToolType::SHOVEL, 6.0f, 2};
            case 257: return {ToolType::PICKAXE, 6.0f, 2};
            case 258: return {ToolType::AXE, 6.0f, 2};
            case 269: return {ToolType::SHOVEL, 2.0f, 0};
            case 270: return {ToolType::PICKAXE, 2.0f, 0};
            case 271: return {ToolType::AXE, 2.0f, 0};
            case 273: return {ToolType::SHOVEL, 4.0f, 1};
            case 274: return {ToolType::PICKAXE, 4.0f, 1};
            case 275: return {ToolType::AXE, 4.0f, 1};
            case 277: return {ToolType::SHOVEL, 8.0f, 3};
            case 278: return {ToolType::PICKAXE, 8.0f, 3};
            case 279: return {ToolType::AXE, 8.0f, 3};
            case 284: return {ToolType::SHOVEL, 12.0f, 0};
            case 285: return {ToolType::PICKAXE, 12.0f, 0};
            case 286: return {ToolType::AXE, 12.0f, 0};
            case 267: case 268: case 272: case 276: case 283:
                return {ToolType::SWORD, 1.5f, 0};
            case 359: return {ToolType::SHEARS, 1.0f, 0};
            default: return {};
        }
    }

    static bool isShovelEffective(const Block block) {
        switch (blockId(block)) {
            case blockId(BLOCK_CLAY): case blockId(BLOCK_DIRT): case blockId(BLOCK_FARMLAND):
            case blockId(BLOCK_GRASS): case blockId(BLOCK_GRAVEL): case blockId(BLOCK_MYCELIUM):
            case blockId(BLOCK_SAND): case blockId(BLOCK_SNOW): case blockId(BLOCK_SNOW_LAYER):
            case blockId(BLOCK_SOUL_SAND): return true;
            default: return false;
        }
    }

    static bool isPlantMaterial(const Block block) {
        switch (blockId(block)) {
            case blockId(BLOCK_SAPLING): case blockId(BLOCK_TALLGRASS): case blockId(BLOCK_DEADBUSH):
            case blockId(BLOCK_YELLOW_FLOWER): case blockId(BLOCK_RED_FLOWER):
            case blockId(BLOCK_BROWN_MUSHROOM): case blockId(BLOCK_RED_MUSHROOM):
            case blockId(BLOCK_WHEAT): case blockId(BLOCK_REEDS): case blockId(BLOCK_VINE):
            case blockId(BLOCK_PUMPKIN_STEM): case blockId(BLOCK_MELON_STEM):
            case blockId(BLOCK_NETHER_WART): case blockId(BLOCK_COCOA):
            case blockId(BLOCK_CARROTS): case blockId(BLOCK_POTATOES):
            case blockId(BLOCK_DOUBLE_PLANT): return true;
            default: return false;
        }
    }

    static bool isAxeEffective(const Block block) {
        if (isPlantMaterial(block)) return true;
        switch (blockId(block)) {
            case blockId(BLOCK_PLANKS): case blockId(BLOCK_LOG): case blockId(BLOCK_LOG2):
            case blockId(BLOCK_BOOKSHELF): case blockId(BLOCK_CHEST): case blockId(BLOCK_TRAPPED_CHEST):
            case blockId(BLOCK_CRAFTING_TABLE): case blockId(BLOCK_LADDER): case blockId(BLOCK_PUMPKIN):
            case blockId(BLOCK_LIT_PUMPKIN): case blockId(BLOCK_MELON_BLOCK):
            case blockId(BLOCK_WOODEN_SLAB): case blockId(BLOCK_DOUBLE_WOODEN_SLAB):
            case blockId(BLOCK_OAK_STAIRS): case blockId(BLOCK_SPRUCE_STAIRS):
            case blockId(BLOCK_BIRCH_STAIRS): case blockId(BLOCK_JUNGLE_STAIRS):
            case blockId(BLOCK_ACACIA_STAIRS): case blockId(BLOCK_DARK_OAK_STAIRS):
            case blockId(BLOCK_FENCE): case blockId(BLOCK_SPRUCE_FENCE): case blockId(BLOCK_BIRCH_FENCE):
            case blockId(BLOCK_JUNGLE_FENCE): case blockId(BLOCK_DARK_OAK_FENCE): case blockId(BLOCK_ACACIA_FENCE):
            case blockId(BLOCK_FENCE_GATE): case blockId(BLOCK_SPRUCE_FENCE_GATE):
            case blockId(BLOCK_BIRCH_FENCE_GATE): case blockId(BLOCK_JUNGLE_FENCE_GATE):
            case blockId(BLOCK_DARK_OAK_FENCE_GATE): case blockId(BLOCK_ACACIA_FENCE_GATE):
            case blockId(BLOCK_WOODEN_DOOR): case blockId(BLOCK_SPRUCE_DOOR): case blockId(BLOCK_BIRCH_DOOR):
            case blockId(BLOCK_JUNGLE_DOOR): case blockId(BLOCK_ACACIA_DOOR): case blockId(BLOCK_DARK_OAK_DOOR):
            case blockId(BLOCK_TRAPDOOR): case blockId(BLOCK_STANDING_SIGN): case blockId(BLOCK_WALL_SIGN):
            case blockId(BLOCK_WOODEN_PRESSURE_PLATE): case blockId(BLOCK_WOODEN_BUTTON): return true;
            default: return false;
        }
    }

    static bool isPickaxeEffective(const Block block) {
        switch (blockId(block)) {
            case blockId(BLOCK_STONE): case blockId(BLOCK_COBBLESTONE): case blockId(BLOCK_COAL_ORE):
            case blockId(BLOCK_IRON_ORE): case blockId(BLOCK_GOLD_ORE): case blockId(BLOCK_LAPIS_ORE):
            case blockId(BLOCK_REDSTONE_ORE): case blockId(BLOCK_LIT_REDSTONE_ORE):
            case blockId(BLOCK_DIAMOND_ORE): case blockId(BLOCK_EMERALD_ORE): case blockId(BLOCK_QUARTZ_ORE):
            case blockId(BLOCK_OBSIDIAN): case blockId(BLOCK_NETHERRACK): case blockId(BLOCK_END_STONE):
            case blockId(BLOCK_SANDSTONE): case blockId(BLOCK_RED_SANDSTONE):
            case blockId(BLOCK_MOSSY_COBBLESTONE): case blockId(BLOCK_STONEBRICK):
            case blockId(BLOCK_BRICK_BLOCK): case blockId(BLOCK_NETHER_BRICK):
            case blockId(BLOCK_GOLD_BLOCK): case blockId(BLOCK_IRON_BLOCK): case blockId(BLOCK_LAPIS_BLOCK):
            case blockId(BLOCK_DIAMOND_BLOCK): case blockId(BLOCK_EMERALD_BLOCK):
            case blockId(BLOCK_REDSTONE_BLOCK): case blockId(BLOCK_COAL_BLOCK):
            case blockId(BLOCK_STONE_SLAB): case blockId(BLOCK_DOUBLE_STONE_SLAB):
            case blockId(BLOCK_STONE_SLAB2): case blockId(BLOCK_DOUBLE_STONE_SLAB2):
            case blockId(BLOCK_STONE_STAIRS): case blockId(BLOCK_BRICK_STAIRS):
            case blockId(BLOCK_STONE_BRICK_STAIRS): case blockId(BLOCK_NETHER_BRICK_STAIRS):
            case blockId(BLOCK_SANDSTONE_STAIRS): case blockId(BLOCK_QUARTZ_STAIRS):
            case blockId(BLOCK_RED_SANDSTONE_STAIRS): case blockId(BLOCK_ANVIL):
            case blockId(BLOCK_RAIL): case blockId(BLOCK_GOLDEN_RAIL):
            case blockId(BLOCK_DETECTOR_RAIL): case blockId(BLOCK_ACTIVATOR_RAIL):
            case blockId(BLOCK_ICE): case blockId(BLOCK_PACKED_ICE): case blockId(BLOCK_PRISMARINE): return true;
            default: return false;
        }
    }

    static float toolStrength(const Block block, const ToolProperties tool) {
        switch (tool.type) {
            case ToolType::PICKAXE: return isPickaxeEffective(block) ? tool.efficiency : 1.0f;
            case ToolType::AXE: return isAxeEffective(block) ? tool.efficiency : 1.0f;
            case ToolType::SHOVEL: return isShovelEffective(block) ? tool.efficiency : 1.0f;
            case ToolType::SWORD: return blockId(block) == blockId(BLOCK_WEB) ? 15.0f :
                (blockId(block) == blockId(BLOCK_LEAVES) || blockId(block) == blockId(BLOCK_LEAVES2)
                 || isPlantMaterial(block) || blockId(block) == blockId(BLOCK_PUMPKIN)
                 || blockId(block) == blockId(BLOCK_LIT_PUMPKIN)
                 || blockId(block) == blockId(BLOCK_MELON_BLOCK) ? 1.5f : 1.0f);
            case ToolType::SHEARS:
                if (blockId(block) == blockId(BLOCK_WEB) || blockId(block) == blockId(BLOCK_LEAVES)
                    || blockId(block) == blockId(BLOCK_LEAVES2)) return 15.0f;
                return blockId(block) == blockId(BLOCK_WOOL) ? 5.0f : 1.0f;
            case ToolType::NONE: return 1.0f;
        }
        return 1.0f;
    }

    static int requiredHarvestLevel(const Block block) {
        switch (blockId(block)) {
            case blockId(BLOCK_OBSIDIAN): return 3;
            case blockId(BLOCK_DIAMOND_ORE): case blockId(BLOCK_DIAMOND_BLOCK):
            case blockId(BLOCK_EMERALD_ORE): case blockId(BLOCK_EMERALD_BLOCK):
            case blockId(BLOCK_GOLD_ORE): case blockId(BLOCK_GOLD_BLOCK):
            case blockId(BLOCK_REDSTONE_ORE): case blockId(BLOCK_LIT_REDSTONE_ORE): return 2;
            case blockId(BLOCK_IRON_ORE): case blockId(BLOCK_IRON_BLOCK):
            case blockId(BLOCK_LAPIS_ORE): case blockId(BLOCK_LAPIS_BLOCK): return 1;
            default: return isPickaxeEffective(block) ? 0 : -1;
        }
    }

    static bool canHarvest(const Block block, const ToolProperties tool) {
        if (blockId(block) == blockId(BLOCK_WEB) || blockId(block) == blockId(BLOCK_REDSTONE_WIRE)
            || blockId(block) == blockId(BLOCK_TRIPWIRE))
            return tool.type == ToolType::SWORD || tool.type == ToolType::SHEARS;
        if (blockId(block) == blockId(BLOCK_SNOW) || blockId(block) == blockId(BLOCK_SNOW_LAYER))
            return tool.type == ToolType::SHOVEL;
        const int level = requiredHarvestLevel(block);
        return level < 0 || (tool.type == ToolType::PICKAXE && tool.harvestLevel >= level);
    }

    float relativeHardness(const Block block) const {
        const float value = hardness(block);
        if (value < 0.0f) return 0.0f;
        if (value == 0.0f) return 1.0f;

        const ToolProperties tool = getToolProperties(miningItemId);
        float strength = toolStrength(block, tool);
        if (mc->player->isInWater()) strength /= 5.0f;
        if (!mc->player->onGround) strength /= 5.0f;
        return strength / value / (canHarvest(block, tool) ? 30.0f : 100.0f);
    }
};
