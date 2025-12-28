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

class NetHandlerLogin : public NetHandler {
public:
    std::string username;
    std::string uuid;
    std::vector<uint8_t> serverPublicKey;
    std::vector<uint8_t> verifyToken;

    mutable bool receivedCompPacket = false;

    explicit NetHandlerLogin(NetClient* client, const std::string& username)
        : NetHandler(client), username(username) { }

    void handlePacket(const ClientBoundPacket& packet) override {
        // std::cout << "[Login] Packet ID: " << packet.id << ", length: " << packet.data.size() << "\n";
        switch (packet.id) {
            case 0x00: { // Disconnect
                const auto p = S00PacketDisconnect::deserialize(packet.data);
                // std::cout << "[Login] Disconnected: " << p.reason << "\n";
                break;
            }
            case 0x01: { // Encryption request
                const auto p = S01PacketEncryptionRequest::deserialize(packet.data);
                serverPublicKey = p.publicKeyEncoded;
                verifyToken = p.verifyToken;
                // std::cout << "[Login] Received encryption request, serverId: " << p.hashedServerId << "\n";
                break;
            }
            case 0x02: { // Login success
                const auto p = S02PacketLoginSuccess::deserialize(packet.data);
                uuid = p.uuid;
                username = p.username;
                // std::cout << "[Login] Login success! UUID: " << uuid << ", Username: " << username << "\n";
                const auto playHandler = std::make_shared<NetHandlerPlay>(client, uuid, username);
                client->setHandler(playHandler);
                break;
            }
            // case 0x03: { // Enable compression
            //     const auto p = S03PacketEnableCompression::deserialize(packet.data);
            //     client->stream_.setCompression(p.compressionThreshold);
            //     std::cout << "[Login] Enable compression: " << p.compressionThreshold << "\n";
            //     break;
            // }
            default:
                // std::cout << "[Login] Unknown packet ID: 0x" << std::hex << packet.id << "\n";
                break;
        }
    }

    [[nodiscard]] const char* getName() const override { return "NetHandlerLogin"; }

    [[nodiscard]] bool preManage(const ClientBoundPacket &packet) const override {
        if (packet.id == 0x03 && !receivedCompPacket) {
            receivedCompPacket = true;
            const S03PacketEnableCompression compPacket = S03PacketEnableCompression::deserialize(packet.data);
            client->stream_.setCompression(compPacket.compressionThreshold);
            // std::cout << "[Login] Enable compression: " << compPacket.compressionThreshold << "\n";
            return true; // don't enqueue the Set Compression packet
        }
        return false;
    }

};
