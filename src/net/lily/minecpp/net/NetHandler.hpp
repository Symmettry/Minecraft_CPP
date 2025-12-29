//
// Created by lily on 12/27/25.
//

#ifndef MINECRAFTCLIENT_NETHANDLER_HPP
#define MINECRAFTCLIENT_NETHANDLER_HPP
#include "packets/ClientBoundPacket.hpp"

struct Minecraft;
struct NetClient;

class NetHandler {
public:
    NetClient *client;
    Minecraft *mc;
    explicit NetHandler(NetClient* client, Minecraft* mc) : client(client), mc(mc) {}

    virtual ~NetHandler() = default;

    virtual void handlePacket(const ClientBoundPacket& packet) = 0;
    virtual void tick() {}
    [[nodiscard]] virtual const char* getName() const { return "NetHandler"; }

    [[nodiscard]] virtual bool isDone() const = 0;

};


#endif //MINECRAFTCLIENT_NETHANDLER_HPP