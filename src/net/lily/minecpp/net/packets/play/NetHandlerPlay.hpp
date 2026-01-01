//
// Created by lily on 12/27/25.
//

#ifndef MINECRAFTCLIENT_NETHANDLERPLAY_HPP
#define MINECRAFTCLIENT_NETHANDLERPLAY_HPP
#include <string>
#include <utility>

#include "client/C00PacketKeepAlive.hpp"
#include "client/C03PacketPlayer.hpp"
#include "client/C17PacketCustomPayload.hpp"
#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/net/NetHandler.hpp"
#include "net/lily/minecpp/net/NetClient.hpp"
#include "net/lily/minecpp/util/ChatHistory.hpp"
#include "net/lily/minecpp/util/Math.hpp"
#include "net/lily/minecpp/world/WorldClient.hpp"
#include "server/S00PacketKeepAlive.hpp"
#include "server/S01PacketJoinGame.hpp"
#include "server/S02PacketChat.hpp"
#include "server/S03PacketTimeUpdate.hpp"
#include "server/S04PacketEntityEquipment.hpp"
#include "server/S05PacketSpawnPosition.hpp"
#include "server/S06PacketUpdateHealth.hpp"
#include "server/S07PacketRespawn.hpp"
#include "server/S08PacketPlayerPosLook.hpp"
#include "server/S1FPacketSetExperience.hpp"
#include "server/S20PacketEntityProperties.hpp"
#include "server/S21PacketChunkData.hpp"
#include "server/S26PacketMapChunkBulk.hpp"

class NetHandlerPlay : public NetHandler {
public:

    mutable bool doneLoadingTerrain = false;
    bool isDone() const override {
        return doneLoadingTerrain;
    }

    std::string uuid, username;

    explicit NetHandlerPlay(NetClient* client, std::string uuid, std::string username, Minecraft* mc)
    : NetHandler(client, mc), uuid(std::move(uuid)), username(std::move(username)) {}

    void sendKeepAlive(const int key) const {
        client->sendPacket(C00PacketKeepAlive{key});
    }

    void processChunk(const int cx, const int cz, const std::vector<uint8_t>& data) const {
        const auto chunk = mc->world->getOrMakeChunk(cx, cz);
        size_t offset = 0;

        for (int sectionY = 0; sectionY < 16; ++sectionY) {
            for (int y = 0; y < 16; ++y) {
                for (int z = 0; z < 16; ++z) {
                    for (int x = 0; x < 16; ++x) {
                        if (offset + 2 > data.size()) break;
                        const uint16_t rawBlock = data[offset] | (data[offset + 1] << 8);
                        offset += 2;

                        if ((rawBlock & 0xFFF) == 0) continue; // ID 0 = air

                        const int worldY = sectionY * 16 + y;
                        chunk->setBlock(x, worldY, z, rawBlock); // store exactly as-is
                    }
                }
            }
        }

        chunk->queueMesh(mc->renderer->blockAtlas);
    }

    void handleKeepAlive(const S00PacketKeepAlive& p) const {
        sendKeepAlive(p.key);
    }

    void handleJoinGame(const S01PacketJoinGame& packet) const {
        mc->world = new WorldClient(this, WorldSettings{ 0L, packet.gameType, false, packet.hardcoreMode, packet.worldType }, packet.dimension, packet.difficulty);
        mc->world->entities.push_back(mc->player);

        mc->player->entityId = packet.entityId;
        mc->player->gameType = packet.gameType;
        mc->player->dimension = packet.dimension;
        mc->player->reducedDebug = packet.reducedDebugInfo;
        mc->settings->difficulty = packet.difficulty;
        mc->maxPlayers = packet.maxPlayers;

        mc->settings->sendSettingsToServer();
        client->sendPacket(C17PacketCustomPayload{"MC|BRAND", "vanilla"});
    }

    void handleChat(const S02PacketChat& p) const {
        ChatHistory::addChat(p.chatMessage);
    }

    void handleTimeUpdate(const S03PacketTimeUpdate& p) const {
        mc->world->totalTime = p.totalWorldTime;
        mc->world->worldTime = p.worldTime;
    }

    void handleEntityEquipment(const S04PacketEntityEquipment& packet) const {
        // todo
    }

    void handleSpawnPosition(const S05PacketSpawnPosition& packet) {
        // todo
    }

    void handleUpdateHealth(const S06PacketUpdateHealth& p) const {
        mc->player->health = p.health;
        mc->player->foodLevel = p.foodLevel;
        mc->player->foodSaturationLevel = p.saturationLevel;
    }

    void handleRespawn(const S07PacketRespawn& packet) {
        // todo
    }

    void handlePlayerPosLook(const S08PacketPlayerPosLook& p) const {
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
        client->sendPacket(C06PacketPlayerPosLook{sx, sy, sz, syaw, spitch, false});

        mc->player->suppressPhysics = true;

        if (!doneLoadingTerrain) {
            mc->player->lastPos = mc->player->position;
            doneLoadingTerrain = true;
            mc->displayGuiScreen(nullptr);
        }
    }

    void handleSetExperience(const S1FPacketSetExperience& p) const {
        mc->player->setXPStats(p.experience, p.experienceTotal, p.experienceLevel);
    }

    void handleEntityProperties(const S20PacketEntityProperties& packet) const {
        // todo
    }

    void handleChunkData(const S21PacketChunkData& p) const {
        processChunk(p.chunkX, p.chunkZ, p.getData());
    }

    void handleMapChunkBulk(const S26PacketMapChunkBulk& bulk) const {
        // printf("Received bulk chunks\n");
        for (uint32_t i = 0; i < bulk.getChunkCount(); ++i) {
            processChunk(bulk.getChunkX(i), bulk.getChunkZ(i), bulk.getChunkData(i).data);
        }
        // for (auto &val: mc->world->chunks | std::views::values) {
        //     val.generateMesh(mc->renderer->blockAtlas);
        //     val.uploadMesh();
        // }
    }

    void handlePacket(const ClientBoundPacket& packet) override {
        // printf("[NetHandlerPlay] Handling packet S%s\n", Math::toHexString(packet.id, true).c_str());
        switch (packet.id) {
            case 0x00: handleKeepAlive(S00PacketKeepAlive::deserialize(packet.data)); break;
            case 0x01: handleJoinGame(S01PacketJoinGame::deserialize(packet.data)); break;
            case 0x02: handleChat(S02PacketChat::deserialize(packet.data)); break;
            case 0x03: handleTimeUpdate(S03PacketTimeUpdate::deserialize(packet.data)); break;
            // case 0x04: handleEntityEquipment(S04PacketEntityEquipment::deserialize(packet.data)); break;
            case 0x05: handleSpawnPosition(S05PacketSpawnPosition::deserialize(packet.data)); break;
            case 0x06: handleUpdateHealth(S06PacketUpdateHealth::deserialize(packet.data)); break;
            case 0x07: handleRespawn(S07PacketRespawn::deserialize(packet.data)); break;
            case 0x08: handlePlayerPosLook(S08PacketPlayerPosLook::deserialize(packet.data)); break;
            //<...>
            case 0x1F: handleSetExperience(S1FPacketSetExperience::deserialize(packet.data)); break;
            //<...>
            case 0x20: handleEntityProperties(S20PacketEntityProperties::deserialize(packet.data)); break;
            case 0x21: handleChunkData(S21PacketChunkData::deserialize(packet.data)); break;
            //<...>
            case 0x26: handleMapChunkBulk(S26PacketMapChunkBulk::deserialize(packet.data)); break;
            //<...>
            default: {
                printf("Unhandled packet: %s\n", Math::toHexString(packet.id).c_str());
                break;
            }
        }
    }

    void tick() override {

    }

    [[nodiscard]] const char* getName() const override {
        return "NetHandlerPlay";
    }

};

#endif //MINECRAFTCLIENT_NETHANDLERPLAY_HPP