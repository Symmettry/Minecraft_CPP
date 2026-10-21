//
// created by lily on 12/27/25
//

#ifndef MINECRAFTCLIENT_NETHANDLERPLAY_HPP
#define MINECRAFTCLIENT_NETHANDLERPLAY_HPP
#include <array>
#include <string>
#include <unordered_map>
#include <utility>

#include "client/C00PacketKeepAlive.hpp"
#include "client/C03PacketPlayer.hpp"
#include "client/C17PacketCustomPayload.hpp"
#include "client/C0FPacketConfirmTransaction.hpp"
#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/net/NetHandler.hpp"
#include "net/lily/minecpp/net/NetClient.hpp"
#include "net/lily/minecpp/util/ChatHistory.hpp"
#include "net/lily/minecpp/util/PlayerList.hpp"
#include "net/lily/minecpp/util/Scoreboard.hpp"
#include "net/lily/minecpp/menu/impl/GuiChest.hpp"
#include "net/lily/minecpp/world/WorldClient.hpp"
#include "server/S00PacketKeepAlive.hpp"
#include "server/S01PacketJoinGame.hpp"
#include "server/S02PacketChat.hpp"
#include "server/S03PacketTimeUpdate.hpp"
#include "server/S04PacketEntityEquipment.hpp"
#include "server/S1BPacketEntityAttach.hpp"
#include "server/S1DPacketEntityEffect.hpp"
#include "server/S1EPacketRemoveEntityEffect.hpp"
#include "server/S2APacketParticles.hpp"
#include "server/S2BPacketChangeGameState.hpp"
#include "server/S2DPacketOpenWindow.hpp"
#include "server/S2EPacketCloseWindow.hpp"
#include "server/S39PacketPlayerAbilities.hpp"
#include "server/S3BPacketScoreboardObjective.hpp"
#include "server/S3CPacketUpdateScore.hpp"
#include "server/S3DPacketDisplayScoreboard.hpp"
#include "server/S3EPacketTeams.hpp"
#include "server/S05PacketSpawnPosition.hpp"
#include "server/S06PacketUpdateHealth.hpp"
#include "server/S07PacketRespawn.hpp"
#include "server/S08PacketPlayerPosLook.hpp"
#include "server/S09PacketHeldItemChange.hpp"
#include "server/S1FPacketSetExperience.hpp"
#include "server/S20PacketEntityProperties.hpp"
#include "server/S21PacketChunkData.hpp"
#include "server/S22PacketMultiBlockChange.hpp"
#include "server/S23PacketBlockChange.hpp"
#include "server/S24PacketBlockAction.hpp"
#include "server/S25PacketBlockBreakAnim.hpp"
#include "server/S26PacketMapChunkBulk.hpp"
#include "server/S2FPacketSetSlot.hpp"
#include "server/S30PacketWindowItems.hpp"
#include "server/S32PacketConfirmTransaction.hpp"
#include "server/S35PacketUpdateTileEntity.hpp"
#include "server/S0CPacketSpawnPlayer.hpp"
#include "server/S0BPacketAnimation.hpp"
#include "server/S0DPacketCollectItem.hpp"
#include "server/S0EPacketSpawnObject.hpp"
#include "server/S0FPacketSpawnMob.hpp"
#include "server/S13PacketDestroyEntities.hpp"
#include "server/S12PacketEntityVelocity.hpp"
#include "server/S15PacketEntityRelMove.hpp"
#include "server/S16PacketEntityLook.hpp"
#include "server/S17PacketEntityLookMove.hpp"
#include "server/S18PacketEntityTeleport.hpp"
#include "server/S19PacketEntityHeadLook.hpp"
#include "server/S1APacketEntityStatus.hpp"
#include "server/S1CPacketEntityMetadata.hpp"
#include "server/S38PacketPlayerListItem.hpp"
#include "net/lily/minecpp/client/entity/EntityOtherPlayerMP.hpp"
#include "net/lily/minecpp/entity/item/EntityItem.hpp"
#include "net/lily/minecpp/entity/EntityMob.hpp"
#include "net/lily/minecpp/util/SkinProfile.hpp"

class NetHandlerPlay final : public NetHandler {
    static std::string uuidKey(const std::array<uint8_t, 16>& uuid) {
        return {reinterpret_cast<const char*>(uuid.data()), uuid.size()};
    }

    mutable std::unordered_map<std::string, std::string> playerNames;
    mutable std::unordered_map<std::string, SkinProfile> playerSkins;
public:

    mutable bool doneLoadingTerrain = false;
    bool isDone() const override {
        return doneLoadingTerrain;
    }

    std::string uuid, username;

    explicit NetHandlerPlay(NetClient* client, std::string uuid, std::string username, Minecraft* mc)
    : NetHandler(client, mc), uuid(std::move(uuid)), username(std::move(username)) {
        mc->player->username = this->username;
        mc->player->skinUrl.clear();
        mc->player->slimSkin = false;
        Scoreboard::clear();
        std::lock_guard lock(PlayerList::mutex);
        PlayerList::entries.clear();
    }

    void sendKeepAlive(const int key) const {
        client->sendPacket(C00PacketKeepAlive{key});
    }

    void processChunk(const int cx, const int cz, const S21PacketChunkData::Chunk& chunkData,
                      const bool replaceMissingSections) const {
        const auto chunk = mc->world->getOrMakeChunk(cx, cz);
        chunk->dataReady.store(false, std::memory_order_release);
        {
            std::lock_guard meshLock(chunk->meshMutex);
            if (replaceMissingSections) chunk->setBiomes(chunkData.biomes);

            for (int sectionY = 0; sectionY < 16; ++sectionY) {
                const auto&[blocks, blockLight, skyLight, isSkipped] = chunkData.sections[sectionY];
                if (isSkipped) {
                    if (replaceMissingSections) chunk->clearSection(sectionY);
                    continue;
                }

                chunk->clearSection(sectionY);
                chunk->setSectionLight(sectionY, blockLight, skyLight);
                const int baseY = sectionY * 16;

                for (int y = 0; y < 16; ++y) {
                    const int worldY = baseY + y;

                    for (int z = 0; z < 16; ++z) {
                        for (int x = 0; x < 16; ++x) {
                            const Block block = blocks[y*16*16 + z*16 + x];
                            if (blockId(block) == 0) continue;

                            chunk->setBlock(x, worldY, z, block);
                        }
                    }
                }
            }

            if (replaceMissingSections) {
                chunk->initializeEmptySectionSkyLight(mc->player->dimension == 0);
            }
            chunk->relightSkylight();
        }
        chunk->dataReady.store(true, std::memory_order_release);

        chunk->queueMesh(mc->renderer->blockAtlas);
        constexpr int neighborOffsets[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        for (const auto& offset : neighborOffsets) {
            const auto neighbor = mc->world->getLoadedChunk(cx + offset[0], cz + offset[1]);
            if (neighbor != nullptr) neighbor->queueMesh(mc->renderer->blockAtlas);
        }
    }

    void handleKeepAlive(const S00PacketKeepAlive& p) const {
        sendKeepAlive(p.key);
    }

    void handleJoinGame(const S01PacketJoinGame& packet) const {
        mc->world = new WorldClient(this, WorldSettings{ 0L, packet.gameType, false, packet.hardcoreMode, packet.worldType }, packet.dimension, packet.difficulty);
        mc->world->addEntity(mc->player);

        mc->player->entityId = packet.entityId;
        mc->player->gameType = packet.gameType;
        mc->player->dimension = packet.dimension;
        mc->player->reducedDebug = packet.reducedDebugInfo;
        mc->player->hardcore = packet.hardcoreMode;
        mc->player->ridingEntity = nullptr;
        mc->player->ridingEntityId = -1;
        mc->player->currentHotbarSlot = 0;
        mc->settings->difficulty = packet.difficulty;
        mc->maxPlayers = packet.maxPlayers;

        mc->settings->sendSettingsToServer();
        client->sendPacket(C17PacketCustomPayload{"MC|BRAND", "vanilla"});
    }

    void handleChat(const S02PacketChat& p) const {
        const int visibility = mc->settings->getInt("chatVisibility");
        if (p.type == 2) {
            if (visibility != 2) ChatHistory::setActionBar(p.chatMessage);
            return;
        }
        if (visibility == 2 || (visibility == 1 && p.type == 0)) return;
        ChatHistory::addChat(p.chatMessage);
    }

    void handleTimeUpdate(const S03PacketTimeUpdate& p) const {
        mc->world->totalTime = p.totalWorldTime;
        mc->world->daylightCycle = p.worldTime >= 0;
        mc->world->worldTime = p.worldTime < 0 ? -p.worldTime : p.worldTime;
    }

    void handleEntityEquipment(const S04PacketEntityEquipment& packet) const {
        if (packet.slot < 0 || packet.slot > 4) return;
        const std::shared_ptr<Entity> entity = mc->world->getEntityById(packet.entityId);
        if (entity == nullptr || entity->getType() != PLAYER || entity.get() == mc->player.get()) return;
        const auto player = std::static_pointer_cast<EntityPlayer>(entity);
        if (packet.slot == 0) player->heldItem = packet.stack;
        else player->armor[packet.slot - 1] = packet.stack;
    }

    void handleParticles(const S2APacketParticles& packet) const {
        mc->renderer->queueServerParticle(packet.type, glm::dvec3(packet.position), glm::dvec3(packet.offset),
            packet.speed, packet.count, packet.arguments);
    }

    void handleChangeGameState(const S2BPacketChangeGameState& packet) const {
        if (packet.reason != 3) return; // game mode change
        const GameType::Value gameType = GameType::fromID(static_cast<int>(packet.value + 0.5f));
        mc->player->gameType = gameType;
        if (gameType == GameType::SPECTATOR) mc->player->isFlying = true;
        mc->requestGuiRefresh();
    }

    void handlePlayerAbilities(const S39PacketPlayerAbilities& packet) const {
        mc->player->allowFlying = (packet.flags & 4) != 0;
        mc->player->isFlying = (packet.flags & 2) != 0 && mc->player->allowFlying;
        mc->player->flySpeed = packet.flySpeed;
    }

    void handleSpawnPosition(const S05PacketSpawnPosition& packet) const {
        // todo
    }

    void handleUpdateHealth(const S06PacketUpdateHealth& p) const {
        if (!mc->isCallingFromMainThread()) {
            mc->addScheduledTask([this, p] { handleUpdateHealth(p); });
            return;
        }
        const bool wasDead = mc->player->health <= 0.0f;
        mc->player->setPlayerSPHealth(p.health);
        mc->player->foodLevel = p.foodLevel;
        mc->player->foodSaturationLevel = p.saturationLevel;
        if (p.health <= 0.0f && !wasDead) mc->requestGuiRefresh();
        else if (p.health > 0.0f && wasDead) mc->requestGuiRefresh();
    }

    void handleRespawn(const S07PacketRespawn& packet) const {
        // chunk packets following S07 are decoded on the network thread and need the new dimension immediately to determine whether skylight arrays are present
        mc->player->dimension = packet.dimensionID;
        if (!mc->isCallingFromMainThread()) {
            mc->addScheduledTask([this, packet] { handleRespawn(packet); });
            return;
        }
        mc->player->gameType = packet.gameType;
        mc->player->resetAfterRespawn();
        mc->renderer->shouldCalcView = true;
        mc->requestGuiRefresh();
    }

    void handlePlayerPosLook(const S08PacketPlayerPosLook& p) const {
        if (!mc->isCallingFromMainThread()) {
            mc->addScheduledTask([this, p] { handlePlayerPosLook(p); });
            return;
        }
        const auto player = mc->player;

        double sx = p.x, sy = p.y, sz = p.z;
        float syaw = p.yaw, spitch = p.pitch;

        if (p.flags.contains(S08PacketPlayerPosLook::EnumFlags::X)) sx += player->position.x;
        else player->velocity.x = 0.0;
        if (p.flags.contains(S08PacketPlayerPosLook::EnumFlags::Y)) sy += player->position.y;
        else player->velocity.y = 0.0;
        if (p.flags.contains(S08PacketPlayerPosLook::EnumFlags::Z)) sz += player->position.z;
        else player->velocity.z = 0.0;

        if (p.flags.contains(S08PacketPlayerPosLook::EnumFlags::X_ROT)) spitch += player->rotation.pitch;
        if (p.flags.contains(S08PacketPlayerPosLook::EnumFlags::Y_ROT)) syaw += player->rotation.yaw;

        mc->player->setPositionAndRotation(sx, sy, sz, syaw, spitch);
        mc->renderer->shouldCalcView = true;
        // 1.8.9 acknowledges the post-set entity values, not the raw packet values. In particular, setRotation has already normalized yaw and pitch at this point
        const double acknowledgedX = player->position.x;
        const double acknowledgedY = player->getBoundingBox().minY;
        const double acknowledgedZ = player->position.z;
        const float acknowledgedYaw = player->rotation.yaw;
        const float acknowledgedPitch = player->rotation.pitch;
        client->sendPacket(C06PacketPlayerPosLook{acknowledgedX, acknowledgedY, acknowledgedZ,
            acknowledgedYaw, acknowledgedPitch, false});
        // synchronize the report cache with the correction. The regular player update still sends its normal flying packet later this tick, just as EntityPlayerSP does in 1.8.9
        player->lastReportedPos = {acknowledgedX, acknowledgedY, acknowledgedZ};
        player->lastReportedRot = {acknowledgedYaw, acknowledgedPitch};
        player->positionUpdateTicks = 0;

        if (!doneLoadingTerrain) {
            mc->player->lastPos = mc->player->position;
            doneLoadingTerrain = true;
            mc->requestGuiRefresh();
        }
    }

    void handleSetExperience(const S1FPacketSetExperience& p) const {
        mc->player->setXPStats(p.experience, p.experienceTotal, p.experienceLevel);
    }

    void applyMetadata(const std::shared_ptr<Entity>& entity, const EntityMetadata& metadata) const {
        if (entity.get() == mc->player.get()) {
            if (metadata.shorts[1].has_value()) mc->player->air = metadata.shorts[1].value();
            if (metadata.floats[17].has_value()) mc->player->absorption = metadata.floats[17].value();
        }
        if (entity->getType() == MOB && metadata.floats[6].has_value())
            entity->health = metadata.floats[6].value();
        if (metadata.bytes[0].has_value()) {
            entity->setSneaking((metadata.flags & 0x02) != 0);
            entity->setSprinting((metadata.flags & 0x08) != 0);
        }
        if (entity->getType() == ITEM && metadata.item.has_value()) {
            const auto item = std::static_pointer_cast<EntityItem>(entity);
            item->itemId = metadata.item->itemId;
            item->count = metadata.item->count;
            item->damage = metadata.item->damage;
            item->enchanted = metadata.item->enchanted;
        } else if (entity->getType() == MOB) {
            const std::shared_ptr<EntityMob> mob = std::static_pointer_cast<EntityMob>(entity);
            EntityMetadata& current = mob->metadata;
            if (metadata.bytes[0].has_value()) current.flags = metadata.flags;
            for (size_t index = 0; index < current.bytes.size(); ++index) {
                if (metadata.bytes[index].has_value()) current.bytes[index] = metadata.bytes[index];
                if (metadata.shorts[index].has_value()) current.shorts[index] = metadata.shorts[index];
                if (metadata.ints[index].has_value()) current.ints[index] = metadata.ints[index];
                if (metadata.floats[index].has_value()) current.floats[index] = metadata.floats[index];
                if (metadata.strings[index].has_value()) current.strings[index] = metadata.strings[index];
                if (metadata.rotations[index].has_value()) current.rotations[index] = metadata.rotations[index];
            }
            mob->refreshArmorStandDimensions();
        }
    }

    void handleSpawnPlayer(const S0CPacketSpawnPlayer& packet) const {
        const auto player = std::make_shared<EntityOtherPlayerMP>(mc,
            packet.position.x, packet.position.y, packet.position.z);
        player->entityId = packet.entityId;
        player->serverPosition = glm::ivec3(glm::round(packet.position * 32.0));
        player->uuid = packet.uuid;
        if (const auto found = playerNames.find(uuidKey(packet.uuid)); found != playerNames.end()) {
            player->username = found->second;
        }
        if (const auto found = playerSkins.find(uuidKey(packet.uuid)); found != playerSkins.end()) {
            player->skinUrl = found->second.url;
            player->slimSkin = found->second.slim;
        }
        player->setRotation(packet.yaw, packet.pitch);
        player->renderYawOffset = packet.yaw;
        player->prevRenderYawOffset = packet.yaw;
        player->rotationYawHead = packet.yaw;
        player->prevRotationYawHead = packet.yaw;
        applyMetadata(player, packet.metadata);
        mc->world->addEntity(player);
    }

    void handleSpawnObject(const S0EPacketSpawnObject& packet) const {
        switch (packet.type) {
            case 10: { // minecart
                const auto cart = std::make_shared<EntityMob>(mc, 210,
                    packet.position.x, packet.position.y, packet.position.z);
                cart->entityId = packet.entityId;
                cart->serverPosition = glm::ivec3(glm::round(packet.position * 32.0));
                cart->velocity = packet.velocity;
                cart->setRotation(packet.yaw, packet.pitch);
                mc->world->addEntity(cart);
                break;
            }
            case 2: {
                const auto item = std::make_shared<EntityItem>(mc,
                    packet.position.x, packet.position.y, packet.position.z);
                item->entityId = packet.entityId;
                item->serverPosition = glm::ivec3(glm::round(packet.position * 32.0));
                item->velocity = packet.velocity;
                item->setRotation(packet.yaw, packet.pitch);
                mc->world->addEntity(item);
                break;
            }
            case 78: {
                const auto armorStand = std::make_shared<EntityMob>(mc, 30,
                    packet.position.x, packet.position.y, packet.position.z);
                armorStand->entityId = packet.entityId;
                armorStand->serverPosition = glm::ivec3(glm::round(packet.position * 32.0));
                armorStand->velocity = packet.velocity;
                armorStand->setRotation(packet.yaw, packet.pitch);
                armorStand->headYaw = armorStand->previousHeadYaw = packet.yaw;
                mc->world->addEntity(armorStand);
                break;
            }
            default:
                break;
        }
    }

    void handleSpawnMob(const S0FPacketSpawnMob& packet) const {
        // bosses need dedicated multipart renderers and are intentionally not represented by the generic path
        if (packet.type == 63 || packet.type == 64) return;
        const auto mob = std::make_shared<EntityMob>(mc, packet.type,
            packet.position.x, packet.position.y, packet.position.z);
        mob->entityId = packet.entityId;
        mob->serverPosition = glm::ivec3(glm::round(packet.position * 32.0));
        mob->velocity = packet.velocity;
        mob->setRotation(packet.yaw, packet.pitch);
        mob->headYaw = mob->previousHeadYaw = packet.headYaw;
        applyMetadata(mob, packet.metadata);
        mc->world->addEntity(mob);
    }

    void handleDestroyEntities(const S13PacketDestroyEntities& packet) const {
        for (const int32_t entityId : packet.entityIds) {
            if (mc->player->ridingEntityId == entityId) {
                mc->player->ridingEntity = nullptr;
                mc->player->ridingEntityId = -1;
                mc->player->vehicleId = -1;
            }
            mc->world->removeEntity(entityId);
        }
    }

    void handleEntityAttach(const S1BPacketEntityAttach& packet) const {
        const std::shared_ptr<Entity> rider = packet.entityId == mc->player->entityId
            ? std::static_pointer_cast<Entity>(mc->player) : mc->world->getEntityById(packet.entityId);
        if (rider == nullptr) return;
        const std::shared_ptr<Entity> vehicle = packet.vehicleId < 0 ? nullptr
            : mc->world->getEntityById(packet.vehicleId);
        rider->vehicleId = vehicle != nullptr ? packet.vehicleId : -1;
        if (rider.get() == mc->player.get()) {
            mc->player->ridingEntity = vehicle.get();
            mc->player->ridingEntityId = rider->vehicleId;
        }
    }

    void handleEntityEffect(const S1DPacketEntityEffect& packet) const {
        if (packet.entityId == mc->player->entityId && packet.effectId < mc->player->activeEffects.size())
            mc->player->activeEffects[packet.effectId] = true;
    }

    void handleRemoveEntityEffect(const S1EPacketRemoveEntityEffect& packet) const {
        if (packet.entityId == mc->player->entityId && packet.effectId < mc->player->activeEffects.size())
            mc->player->activeEffects[packet.effectId] = false;
    }

    void handleEntityVelocity(const S12PacketEntityVelocity& packet) const {
        if (!mc->isCallingFromMainThread()) {
            mc->addScheduledTask([this, packet] { handleEntityVelocity(packet); });
            return;
        }
        if (const std::shared_ptr<Entity> entity = mc->world->getEntityById(packet.entityId))
            entity->velocity = packet.velocity;
    }

    void handleEntityStatus(const S1APacketEntityStatus& packet) const {
        const std::shared_ptr<Entity> entity = mc->world->getEntityById(packet.entityId);
        if (entity == nullptr) return;
        switch (packet.opcode) {
            case 2:
                entity->hurtTime = entity->maxHurtTime = 10;
                entity->hurtResistantTime = 20;
                entity->attackedAtYaw = 0.0f;
                break;
            default:
                break;
        }
    }

    void moveRemoteEntity(const int32_t entityId, const glm::dvec3& position,
                          const float yaw, const float pitch) const {
        const std::shared_ptr<Entity> entity = mc->world->getEntityById(entityId);
        if (entity == nullptr || entity.get() == mc->player.get()) return;
        if (entity->getType() == PLAYER) {
            std::static_pointer_cast<EntityOtherPlayerMP>(entity)->setRemotePosition(
                position.x, position.y, position.z, yaw, pitch, 3);
            return;
        }
        if (entity->getType() == MOB) {
            std::static_pointer_cast<EntityMob>(entity)->setRemotePosition(
                position.x, position.y, position.z, yaw, pitch, 3);
            return;
        }
        entity->lastPos = entity->position;
        entity->setPositionPreservingPrevious(position.x, position.y, position.z);
        entity->setRotation(yaw, pitch);
    }

    void handleRelativeMove(const S15PacketEntityRelMove& packet) const {
        if (const std::shared_ptr<Entity> entity = mc->world->getEntityById(packet.entityId)) {
            entity->onGround = packet.onGround;
            entity->serverPosition += glm::ivec3(glm::round(packet.delta * 32.0));
            moveRemoteEntity(packet.entityId, glm::dvec3(entity->serverPosition) / 32.0,
                entity->rotation.yaw, entity->rotation.pitch);
        }
    }

    void handleEntityLook(const S16PacketEntityLook& packet) const {
        if (const std::shared_ptr<Entity> entity = mc->world->getEntityById(packet.entityId)) {
            entity->onGround = packet.onGround;
            moveRemoteEntity(packet.entityId, glm::dvec3(entity->serverPosition) / 32.0,
                packet.yaw, packet.pitch);
        }
    }

    void handleLookMove(const S17PacketEntityLookMove& packet) const {
        if (const std::shared_ptr<Entity> entity = mc->world->getEntityById(packet.entityId)) {
            entity->onGround = packet.onGround;
            entity->serverPosition += glm::ivec3(glm::round(packet.delta * 32.0));
            moveRemoteEntity(packet.entityId, glm::dvec3(entity->serverPosition) / 32.0,
                packet.yaw, packet.pitch);
        }
    }

    void handleEntityTeleport(const S18PacketEntityTeleport& packet) const {
        if (const std::shared_ptr<Entity> entity = mc->world->getEntityById(packet.entityId)) {
            entity->onGround = packet.onGround;
            entity->serverPosition = glm::ivec3(glm::round(packet.position * 32.0));
            moveRemoteEntity(packet.entityId, packet.position, packet.yaw, packet.pitch);
        }
    }

    void handleHeadLook(const S19PacketEntityHeadLook& packet) const {
        if (const std::shared_ptr<Entity> entity = mc->world->getEntityById(packet.entityId);
            entity != nullptr) {
            switch (entity->getType()) {
                case PLAYER:
                    std::static_pointer_cast<EntityPlayer>(entity)->rotationYawHead = packet.yaw;
                    break;
                case MOB: {
                    const auto mob = std::static_pointer_cast<EntityMob>(entity);
                    mob->previousHeadYaw = mob->headYaw;
                    mob->headYaw = packet.yaw;
                    break;
                }
                default: break;
            }
        }
    }

    void handleEntityMetadata(const S1CPacketEntityMetadata& packet) const {
        if (const std::shared_ptr<Entity> entity = mc->world->getEntityById(packet.entityId))
            applyMetadata(entity, packet.metadata);
    }

    void handleAnimation(const S0BPacketAnimation& packet) const {
        if (packet.animation == 4 || packet.animation == 5) { // critical hit, magic critical hit
            mc->renderer->emitParticlesAtEntity(packet.entityId, packet.animation == 4 ? 9 : 10);
            return;
        }
        if (packet.animation != 0) return;
        if (const std::shared_ptr<Entity> entity = mc->world->getEntityById(packet.entityId);
            entity != nullptr && entity->getType() == PLAYER)
            std::static_pointer_cast<EntityPlayer>(entity)->swingItem();
    }

    void handleCollectItem(const S0DPacketCollectItem& packet) const {
        const std::shared_ptr<Entity> collected = mc->world->getEntityById(packet.collectedEntityId);
        const std::shared_ptr<Entity> collector = packet.collectorEntityId == mc->player->entityId
            ? std::static_pointer_cast<Entity>(mc->player)
            : mc->world->getEntityById(packet.collectorEntityId);
        if (collected != nullptr && collected->getType() == ITEM && collector != nullptr) {
            std::static_pointer_cast<EntityItem>(collected)->beginCollection(collector);
        } else {
            mc->world->removeEntity(packet.collectedEntityId);
        }
    }

    void handlePlayerListItem(const S38PacketPlayerListItem& packet) const {
        for (const S38PacketPlayerListItem::Entry& entry : packet.entries) {
            const std::string key = uuidKey(entry.uuid);
            switch (packet.action) {
                case 0: {
                    SkinProfile skin;
                    for (const auto& property : entry.properties) {
                        if (property.name == "textures") {
                            skin = SkinProfile::fromTextureProperty(property.value);
                            break;
                        }
                    }
                    playerNames[key] = entry.name;
                    playerSkins[key] = skin;
                    {
                        std::lock_guard playerListLock(PlayerList::mutex);
                        PlayerList::entries[key] = {entry.uuid, entry.name, entry.displayName,
                            skin.url, skin.slim, entry.gameMode, entry.ping};
                    }
                    if (mc->player != nullptr && username == entry.name) {
                        mc->player->skinUrl = skin.url;
                        mc->player->slimSkin = skin.slim;
                    }
                    World* const world = mc->world;
                    if (world == nullptr) break;
                    for (const std::shared_ptr<Entity>& entity : world->snapshotEntities()) {
                        if (entity->getType() != PLAYER) continue;
                        if (entity.get() == mc->player.get()) continue;
                        if (const auto player = std::static_pointer_cast<EntityOtherPlayerMP>(entity); player->uuid == entry.uuid) {
                            player->username = entry.name;
                            player->skinUrl = skin.url;
                            player->slimSkin = skin.slim;
                        }
                    }
                    break;
                }
                case 1: { std::lock_guard lock(PlayerList::mutex); PlayerList::entries[key].gameMode = entry.gameMode; break; }
                case 2: { std::lock_guard lock(PlayerList::mutex); PlayerList::entries[key].ping = entry.ping; break; }
                case 3: { std::lock_guard lock(PlayerList::mutex); PlayerList::entries[key].displayName = entry.displayName; break; }
                case 4: {
                    playerNames.erase(key);
                    playerSkins.erase(key);
                    std::lock_guard lock(PlayerList::mutex);
                    PlayerList::entries.erase(key);
                    break;
                }
                default: break;
            }
        }
    }

    static void handleScoreboardObjective(const S3BPacketScoreboardObjective& packet) {
        Scoreboard::updateObjective(packet.name, packet.displayName, packet.renderType, packet.action);
    }

    static void handleUpdateScore(const S3CPacketUpdateScore& packet) {
        Scoreboard::updateScore(packet.player, packet.objective, packet.value, packet.action);
    }

    static void handleDisplayScoreboard(const S3DPacketDisplayScoreboard& packet) {
        Scoreboard::setDisplaySlot(packet.position, packet.objective);
    }

    static void handleTeams(const S3EPacketTeams& packet) {
        ScoreboardTeam values;
        values.displayName = packet.displayName;
        values.prefix = packet.prefix;
        values.suffix = packet.suffix;
        values.nameTagVisibility = packet.nameTagVisibility;
        values.friendlyFlags = packet.friendlyFlags;
        values.color = packet.color;
        Scoreboard::updateTeam(packet.name, packet.action, values, packet.players);
    }

    void handleEntityProperties(const S20PacketEntityProperties& packet) const {
        const std::shared_ptr<Entity> entity = mc->world->getEntityById(packet.entityId);
        if (entity == nullptr) return;
        for (const auto& snapshot : packet.snapshots) {
            const bool maxHealth = snapshot.attributeName == "generic.maxHealth";
            const bool movementSpeed = snapshot.attributeName == "generic.movementSpeed"
                && entity->getType() == PLAYER;
            if (!maxHealth && !movementSpeed) continue;

            // entityLivingBase's sprint boost is installed locally under this UUID. Keep the server-provided attribute value without that modifier, then apply it from isSprinting()
            static constexpr std::array<uint8_t, 16> sprintModifierUuid{
                0x66, 0x2A, 0x6B, 0x8D, 0xDA, 0x3E, 0x4C, 0x1C,
                0x88, 0x13, 0x96, 0xEA, 0x60, 0x97, 0x27, 0x8D};
            const auto applies = [&](const AttributeModifier& modifier) {
                return !movementSpeed || modifier.uuid.bytes != sprintModifierUuid;
            };
            // iAttributeInstance.computeValue: base + additions, then * (1 + sum) and * (1 + each)
            double value = snapshot.baseValue;
            for (const auto& modifier : snapshot.modifiers)
                if (applies(modifier) && modifier.operation == 0) value += modifier.amount;
            double result = value;
            for (const auto& modifier : snapshot.modifiers)
                if (applies(modifier) && modifier.operation == 1) result += value * modifier.amount;
            for (const auto& modifier : snapshot.modifiers)
                if (applies(modifier) && modifier.operation == 2) result *= 1.0 + modifier.amount;
            if (maxHealth) entity->maxHealth = static_cast<float>(result);
            else std::static_pointer_cast<EntityPlayer>(entity)->movementSpeed = result;
        }
    }

    void handleHeldItemChange(const S09PacketHeldItemChange& packet) const {
        if (packet.slot >= 0 && packet.slot < 9) mc->player->currentHotbarSlot = packet.slot;
    }

    void handleOpenWindow(const S2DPacketOpenWindow& packet) const {
        if (!mc->isCallingFromMainThread()) {
            mc->addScheduledTask([this, packet] { handleOpenWindow(packet); });
            return;
        }
        mc->displayGuiScreen(new GuiChest(packet.windowId, packet.titleJson, packet.slotCount));
    }

    void handleCloseWindow(const S2EPacketCloseWindow& packet) const {
        if (!mc->isCallingFromMainThread()) {
            mc->addScheduledTask([this, packet] { handleCloseWindow(packet); });
            return;
        }
        const auto chest = dynamic_cast<GuiChest*>(mc->getCurrentScreen());
        if (chest != nullptr && chest->getWindowId() == packet.windowId) chest->closeFromServer();
    }

    void handleSetSlot(const S2FPacketSetSlot& packet) const {
        if (!mc->isCallingFromMainThread()) {
            mc->addScheduledTask([this, packet] { handleSetSlot(packet); });
            return;
        }
        if (packet.windowId == -1 && packet.slot == -1) {
            mc->player->carriedItem = packet.stack;
            return;
        }
        if (packet.windowId == 0) {
            if (packet.slot >= 0
                && packet.slot < static_cast<int16_t>(mc->player->inventory.size()))
                mc->player->inventory[packet.slot] = packet.stack;
            return;
        }
        const auto chest = dynamic_cast<GuiChest*>(mc->getCurrentScreen());
        if (chest != nullptr && chest->getWindowId() == static_cast<uint8_t>(packet.windowId))
            chest->setSlot(packet.slot, packet.stack);
    }

    void handleConfirmTransaction(const S32PacketConfirmTransaction& packet) const {
        // play packets which vanilla enqueues onto Minecraft's main thread must preserve their receive order. In particular, Grim brackets S08 with S32 transactions; replying here from the network thread can acknowledge the post-teleport transaction before the queued C06 teleport acknowledgement
        if (!mc->isCallingFromMainThread()) {
            mc->addScheduledTask([this, packet] { handleConfirmTransaction(packet); });
            return;
        }
        if (!packet.accepted) client->sendPacket(C0FPacketConfirmTransaction{
            packet.windowId, packet.actionNumber, true});
    }

    void handleWindowItems(const S30PacketWindowItems& packet) const {
        if (!mc->isCallingFromMainThread()) {
            mc->addScheduledTask([this, packet] { handleWindowItems(packet); });
            return;
        }
        if (packet.windowId == 0) {
            const size_t count = std::min(packet.stacks.size(), mc->player->inventory.size());
            std::copy_n(packet.stacks.begin(), count, mc->player->inventory.begin());
            return;
        }
        const auto chest = dynamic_cast<GuiChest*>(mc->getCurrentScreen());
        if (chest != nullptr && chest->getWindowId() == packet.windowId)
            chest->setItems(packet.stacks);
    }

    void handleChunkData(const S21PacketChunkData& packet) const {
        if (packet.groundUp && packet.chunk.bitMask == 0) {
            mc->world->removeChunk(packet.chunkX, packet.chunkZ);
            return;
        }
        processChunk(packet.chunkX, packet.chunkZ, packet.chunk, packet.groundUp);
    }

    void queueChangedChunk(const int chunkX, const int chunkZ,
                           const bool west, const bool east,
                           const bool north, const bool south) const {
        const auto queue = [&](const int x, const int z) {
            const auto chunk = mc->world->getLoadedChunk(x, z);
            if (chunk != nullptr) chunk->queueMesh(mc->renderer->blockAtlas);
        };
        queue(chunkX, chunkZ);
        if (west) queue(chunkX - 1, chunkZ);
        if (east) queue(chunkX + 1, chunkZ);
        if (north) queue(chunkX, chunkZ - 1);
        if (south) queue(chunkX, chunkZ + 1);
    }

    void handleMultiBlockChange(const S22PacketMultiBlockChange& packet) const {
        const auto chunk = mc->world->getLoadedChunk(packet.chunkX, packet.chunkZ);
        if (chunk == nullptr) return;

        bool changed = false;
        uint32_t lightSections = 0;
        {
            std::lock_guard meshLock(chunk->meshMutex);
            for (const auto& update : packet.updates) {
                if (chunk->getBlock(update.x, update.y, update.z) == update.block) continue;
                chunk->setBlock(update.x, update.y, update.z, update.block);
                changed = true;
            }
            if (changed) lightSections = chunk->relightSkylightSections();
        }
        if (!changed) return;
        for (const auto& update : packet.updates)
            mc->world->queueBlockChangeMesh(packet.chunkX, packet.chunkZ, update.x, update.y, update.z,
                lightSections);
    }

    void handleBlockChange(const S23PacketBlockChange& packet) const {
        const int chunkX = static_cast<int>(std::floor(static_cast<double>(packet.position.x) / CHUNK_SIZE));
        const int chunkZ = static_cast<int>(std::floor(static_cast<double>(packet.position.z) / CHUNK_SIZE));
        const auto chunk = mc->world->getLoadedChunk(chunkX, chunkZ);
        if (chunk == nullptr || packet.position.y < 0 || packet.position.y >= WORLD_HEIGHT) return;

        const int localX = World::toLocalX(packet.position.x);
        const int localZ = World::toLocalZ(packet.position.z);
        uint32_t lightSections = 0;
        {
            std::lock_guard meshLock(chunk->meshMutex);
            // the server echoes our own edits; the client already applied and meshed them
            if (chunk->getBlock(localX, packet.position.y, localZ) == packet.block) return;
            chunk->setBlock(localX, packet.position.y, localZ, packet.block);
            lightSections = chunk->relightSkylightSections();
        }
        mc->world->queueBlockChangeMesh(chunkX, chunkZ, localX, packet.position.y, localZ, lightSections);
    }

    void handleBlockAction(const S24PacketBlockAction& packet) const {
        // chests open while somebody has them open: data1 1, data2 = viewer count
        if (packet.data1 == 1 && (blockId(packet.block) == blockId(BLOCK_CHEST)
            || blockId(packet.block) == blockId(BLOCK_TRAPPED_CHEST) || blockId(packet.block) == blockId(BLOCK_ENDER_CHEST)))
            mc->renderer->setChestViewers(packet.position, packet.data2);
        if (packet.data1 == 0 && (blockId(packet.block) == blockId(BLOCK_PISTON)
            || blockId(packet.block) == blockId(BLOCK_STICKY_PISTON)))
            mc->world->queuePistonMovement(packet.position, packet.data2);
        std::lock_guard lock(mc->world->blockEventMutex);
        mc->world->blockActions.push_back({packet.position, packet.data1, packet.data2, packet.block});
        if (mc->world->blockActions.size() > 256) mc->world->blockActions.erase(mc->world->blockActions.begin());
    }

    void handleBlockBreakAnim(const S25PacketBlockBreakAnim& packet) const {
        std::lock_guard lock(mc->world->blockEventMutex);
        if (packet.progress <= 9) {
            mc->world->damagedBlocks[packet.breakerId] = {packet.position, packet.progress};
        } else {
            mc->world->damagedBlocks.erase(packet.breakerId);
        }
    }

    void handleMapChunkBulk(const S26PacketMapChunkBulk& bulk) const {
        for (uint32_t i = 0; i < bulk.getChunkCount(); ++i) {
            processChunk(bulk.getChunkX(i), bulk.getChunkZ(i), bulk.getChunkData(i), true);
        }
    }

    void handleUpdateTileEntity(const S35PacketUpdateTileEntity& packet) const {
        if (packet.blockEntity.has_value())
            mc->world->setBlockEntity(packet.position, packet.blockEntity.value());
    }

    bool handlePacket(const ClientBoundPacket& packet) override {
        // printf("[NetHandlerPlay] Handling packet S%s\n", Math::toHexString(packet.id, true).c_str());
        switch (packet.id) {
            case 0x00: handleKeepAlive(S00PacketKeepAlive::deserialize(packet.buf)); break;
            case 0x01: handleJoinGame(S01PacketJoinGame::deserialize(packet.buf)); break;
            case 0x02: handleChat(S02PacketChat::deserialize(packet.buf)); break;
            case 0x03: handleTimeUpdate(S03PacketTimeUpdate::deserialize(packet.buf)); break;
            case 0x04: handleEntityEquipment(S04PacketEntityEquipment::deserialize(packet.buf)); break;
            case 0x05: handleSpawnPosition(S05PacketSpawnPosition::deserialize(packet.buf)); break;
            case 0x06: handleUpdateHealth(S06PacketUpdateHealth::deserialize(packet.buf)); break;
            case 0x07: handleRespawn(S07PacketRespawn::deserialize(packet.buf)); break;
            case 0x08: handlePlayerPosLook(S08PacketPlayerPosLook::deserialize(packet.buf)); break;
            case 0x09: handleHeldItemChange(S09PacketHeldItemChange::deserialize(packet.buf)); break;
            case 0x0B: handleAnimation(S0BPacketAnimation::deserialize(packet.buf)); break;
            case 0x0C: handleSpawnPlayer(S0CPacketSpawnPlayer::deserialize(packet.buf)); break;
            case 0x0D: handleCollectItem(S0DPacketCollectItem::deserialize(packet.buf)); break;
            case 0x0E: handleSpawnObject(S0EPacketSpawnObject::deserialize(packet.buf)); break;
            case 0x0F: handleSpawnMob(S0FPacketSpawnMob::deserialize(packet.buf)); break;
            case 0x12: handleEntityVelocity(S12PacketEntityVelocity::deserialize(packet.buf)); break;
            case 0x13: handleDestroyEntities(S13PacketDestroyEntities::deserialize(packet.buf)); break;
            case 0x15: handleRelativeMove(S15PacketEntityRelMove::deserialize(packet.buf)); break;
            case 0x16: handleEntityLook(S16PacketEntityLook::deserialize(packet.buf)); break;
            case 0x17: handleLookMove(S17PacketEntityLookMove::deserialize(packet.buf)); break;
            case 0x18: handleEntityTeleport(S18PacketEntityTeleport::deserialize(packet.buf)); break;
            case 0x19: handleHeadLook(S19PacketEntityHeadLook::deserialize(packet.buf)); break;
            case 0x1A: handleEntityStatus(S1APacketEntityStatus::deserialize(packet.buf)); break;
            case 0x1B: handleEntityAttach(S1BPacketEntityAttach::deserialize(packet.buf)); break;
            case 0x1D: handleEntityEffect(S1DPacketEntityEffect::deserialize(packet.buf)); break;
            case 0x1E: handleRemoveEntityEffect(S1EPacketRemoveEntityEffect::deserialize(packet.buf)); break;
            case 0x1C: handleEntityMetadata(S1CPacketEntityMetadata::deserialize(packet.buf)); break;
            case 0x1F: handleSetExperience(S1FPacketSetExperience::deserialize(packet.buf)); break;
            //<...>
            case 0x20: handleEntityProperties(S20PacketEntityProperties::deserialize(packet.buf)); break;
            case 0x21: handleChunkData(S21PacketChunkData::deserialize(packet.buf, mc->player->dimension == 0)); break;
            case 0x22: handleMultiBlockChange(S22PacketMultiBlockChange::deserialize(packet.buf)); break;
            case 0x23: handleBlockChange(S23PacketBlockChange::deserialize(packet.buf)); break;
            case 0x24: handleBlockAction(S24PacketBlockAction::deserialize(packet.buf)); break;
            case 0x25: handleBlockBreakAnim(S25PacketBlockBreakAnim::deserialize(packet.buf)); break;
            case 0x26: handleMapChunkBulk(S26PacketMapChunkBulk::deserialize(packet.buf)); break;
            case 0x2F: handleSetSlot(S2FPacketSetSlot::deserialize(packet.buf)); break;
            case 0x2A: handleParticles(S2APacketParticles::deserialize(packet.buf)); break;
            case 0x2B: handleChangeGameState(S2BPacketChangeGameState::deserialize(packet.buf)); break;
            case 0x2D: handleOpenWindow(S2DPacketOpenWindow::deserialize(packet.buf)); break;
            case 0x2E: handleCloseWindow(S2EPacketCloseWindow::deserialize(packet.buf)); break;
            case 0x30: handleWindowItems(S30PacketWindowItems::deserialize(packet.buf)); break;
            case 0x32: handleConfirmTransaction(S32PacketConfirmTransaction::deserialize(packet.buf)); break;
            case 0x35: handleUpdateTileEntity(S35PacketUpdateTileEntity::deserialize(packet.buf)); break;
            case 0x39: handlePlayerAbilities(S39PacketPlayerAbilities::deserialize(packet.buf)); break;
            case 0x38: handlePlayerListItem(S38PacketPlayerListItem::deserialize(packet.buf)); break;
            case 0x3B: handleScoreboardObjective(S3BPacketScoreboardObjective::deserialize(packet.buf)); break;
            case 0x3C: handleUpdateScore(S3CPacketUpdateScore::deserialize(packet.buf)); break;
            case 0x3D: handleDisplayScoreboard(S3DPacketDisplayScoreboard::deserialize(packet.buf)); break;
            case 0x3E: handleTeams(S3EPacketTeams::deserialize(packet.buf)); break;
            default: {
                // printf("Unhandled packet: S%s\n", Math::toHexString(packet.id).c_str());
                return false;
            }
        }
        return true;
    }

    void tick() override {

    }

    [[nodiscard]] const char* getName() const override {
        return "NetHandlerPlay";
    }

};

#endif //mINECRAFTCLIENT_NETHANDLERPLAY_HPP
