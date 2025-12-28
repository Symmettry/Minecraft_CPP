//
// Created by lily on 12/27/25.
//

#ifndef MINECRAFTCLIENT_NETHANDLERPLAY_HPP
#define MINECRAFTCLIENT_NETHANDLERPLAY_HPP
#include <string>
#include <utility>

#include "client/C00PacketKeepAlive.hpp"
#include "client/C03PacketPlayer.hpp"
#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/net/NetHandler.hpp"
#include "net/lily/minecpp/net/NetClient.hpp"
#include "net/lily/minecpp/util/ChatHistory.hpp"
#include "server/S00PacketKeepAlive.hpp"
#include "server/S02PacketChat.hpp"
#include "server/S03PacketTimeUpdate.hpp"
#include "server/S06PacketUpdateHealth.hpp"
#include "server/S08PacketPlayerPosLook.hpp"
#include "server/S21PacketChunkData.hpp"
#include "server/S26PacketMapChunkBulk.hpp"

class NetHandlerPlay : public NetHandler {
public:

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
                        const uint16_t blockId = data[offset] | (data[offset + 1] << 8);
                        offset += 2;

                        if (blockId == 0) continue;

                        const int worldY = sectionY * 16 + y;
                        chunk->setBlock(x, worldY, z, Material::Dirt); // placeholder
                    }
                }
            }
        }

        chunk->generateMesh(mc->renderer->blockAtlas);
        chunk->uploadMesh();
    }

    void handlePacket(const ClientBoundPacket& packet) override {
        // std::cout << "[Play] Packet ID: " << packet.id << ", length: " << packet.data.size() << "\n";
        switch (packet.id) {
            case 0x00: {
                const auto p = S00PacketKeepAlive::deserialize(packet.data);
                sendKeepAlive(p.key);
                // std::cout << "[Play] Keep Alive: " << p.key << "\n";
                break;
            }
            case 0x01: {
                // todo
                break;
            }
            case 0x02: {
                const auto p = S02PacketChat::deserialize(packet.data);
                ChatHistory::addChat(p.chatMessage);
                break;
            }
            case 0x03: {
                const auto p = S03PacketTimeUpdate::deserialize(packet.data);
                mc->world->totalTime = p.totalWorldTime;
                mc->world->worldTime = p.worldTime;
                break;
            }
            case 0x04: {
                // todo
                break;
            }
            case 0x05: {
                // todo
                break;
            }
            case 0x06: {
                const auto p = S06PacketUpdateHealth::deserialize(packet.data);
                mc->player->health = p.health;
                mc->player->foodLevel = p.foodLevel;
                mc->player->foodSaturationLevel = p.saturationLevel;
                break;
            }
            case 0x07: {
                // todo
                break;
            }
            case 0x08: {
                const auto p = S08PacketPlayerPosLook::deserialize(packet.data);

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
                client->sendPacket(C06PacketPlayerPosLook{sx, player->getBoundingBox().minY, sz, syaw, spitch, false});
                break;
            }
            case 0x21: { // S21PacketChunkData
                const auto p = S21PacketChunkData::deserialize(packet.data);
                processChunk(p.chunkX, p.chunkZ, p.getData());
                break;
            }

            case 0x26: { // S26PacketMapChunkBulk
                printf("Received bulk chunks\n");
                const auto bulk = S26PacketMapChunkBulk::deserialize(packet.data);
                for (uint32_t i = 0; i < bulk.getChunkCount(); ++i) {
                    processChunk(bulk.getChunkX(i), bulk.getChunkZ(i), bulk.getChunkData(i).data);
                }
                break;
            }
        }
    }

    void tick() override {
        // implement tick logic here
    }

    [[nodiscard]] const char* getName() const override {
        return "NetHandlerPlay";
    }

    [[nodiscard]] bool preManage(const ClientBoundPacket &packet) const override {
        return false;
    }

};

#endif //MINECRAFTCLIENT_NETHANDLERPLAY_HPP