//
// Created by lily on 12/27/25.
//

#ifndef MINECRAFTCLIENT_NETHANDLER_HPP
#define MINECRAFTCLIENT_NETHANDLER_HPP
#include "packets/ClientBoundPacket.hpp"

struct NetClient;

class NetHandler {
public:
    NetClient *client;
    explicit NetHandler(NetClient* client) : client(client) {}

    virtual ~NetHandler() = default;

    virtual void handlePacket(const ClientBoundPacket& packet) = 0;
    virtual void tick() {}
    [[nodiscard]] virtual const char* getName() const { return "NetHandler"; }

    [[nodiscard]] virtual bool preManage(const ClientBoundPacket & packet) const {
        return false;
    }
};


#endif //MINECRAFTCLIENT_NETHANDLER_HPP