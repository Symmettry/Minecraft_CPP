//
// Created by lily on 12/27/25.
//

#ifndef MINECRAFTCLIENT_NETHANDLERPLAY_HPP
#define MINECRAFTCLIENT_NETHANDLERPLAY_HPP
#include <string>
#include <utility>

#include "client/C00PacketKeepAlive.hpp"
#include "net/lily/minecpp/net/NetHandler.hpp"
#include "net/lily/minecpp/net/NetClient.hpp"
#include "server/S00PacketKeepAlive.hpp"

class NetHandlerPlay : public NetHandler {
public:

    std::string uuid, username;

    explicit NetHandlerPlay(NetClient* client, std::string uuid, std::string username)
    : NetHandler(client), uuid(std::move(uuid)), username(std::move(username)) {}

    void sendKeepAlive(const int key) const {
        client->sendPacket(C00PacketKeepAlive{key});
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