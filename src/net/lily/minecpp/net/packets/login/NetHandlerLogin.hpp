#pragma once
#include <iostream>
#include <memory>

#include "../../NetHandler.hpp"
#include "client/C00PacketLoginStart.hpp"
#include "net/lily/minecpp/net/NetClient.hpp"
#include "net/lily/minecpp/net/packets/handshake/C00Handshake.hpp"
#include "net/lily/minecpp/net/packets/play/NetHandlerPlay.hpp"
#include "server/S00PacketDisconnect.hpp"
#include "server/S01PacketEncryptionRequest.hpp"
#include "server/S02PacketLoginSuccess.hpp"
#include "server/S03PacketEnableCompression.hpp"

class NetHandlerLogin final : public NetHandler {
public:
    std::string username;
    std::string uuid;
    std::vector<uint8_t> serverPublicKey;
    std::vector<uint8_t> verifyToken;

    explicit NetHandlerLogin(NetClient* client, const std::string& username, Minecraft* mc)
        : NetHandler(client, mc), username(username) { }

    bool handlePacket(const ClientBoundPacket& packet) override {
        switch (packet.id) {
            case 0x00: { // Disconnect
                const auto p = S00PacketDisconnect::deserialize(packet.buf);
                std::cout << "[NetHandlerLogin] Disconnected: " << p.reason << "\n";
                break;
            }
            case 0x01: { // Encryption request
                const auto p = S01PacketEncryptionRequest::deserialize(packet.buf);
                serverPublicKey = p.publicKeyEncoded;
                verifyToken = p.verifyToken;
                std::cout << "[NetHandlerLogin] Received encryption request, serverId: " << p.hashedServerId << "\n";
                break;
            }
            case 0x02: { // Login success
                const auto p = S02PacketLoginSuccess::deserialize(packet.buf);
                uuid = p.uuid;
                username = p.username;
                std::cout << "[NetHandlerLogin] Login success! UUID: " << uuid << ", Username: " << username << "\n";
                const auto playHandler = std::make_shared<NetHandlerPlay>(client, uuid, username, mc);
                client->setHandler(playHandler);
                break;
            }
            case 0x03: { // Enable compression
                const auto p = S03PacketEnableCompression::deserialize(packet.buf);
                client->stream_.setCompression(p.compressionThreshold);
                std::cout << "[NetHandlerLogin] Enable compression: " << p.compressionThreshold << "\n";
                break;
            }
            default:
                std::cout << "[NetHandlerLogin] Unknown packet ID: S" << Math::toHexString(packet.id) << "\n";
                return false;
        }
        return true;
    }

    [[nodiscard]] const char* getName() const override { return "NetHandlerLogin"; }

    bool isDone() const override {
        return true; // unused
    }

};
