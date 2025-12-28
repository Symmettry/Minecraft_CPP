#pragma once
#include "PacketStream.hpp"
#include "packets/Packet.hpp"
#include <string>
#include <thread>
#include <queue>
#include <mutex>
#include <atomic>
#include <memory>

#include "NetHandler.hpp"
#include "packets/ServerBoundPacket.hpp"

struct NetClient {
    NetClient(const std::string& host, uint16_t port, std::string  username);
    ~NetClient();

    bool connect();

    void sendHandshake() const;

    void sendLoginStart() const;

    void disconnect();

    bool pollPacket(ClientBoundPacket &packet);

    bool sendPacket(const ServerBoundPacket &packet) const;

    void tick();

    [[nodiscard]] std::string host() const;
    [[nodiscard]] uint16_t port() const;

    void setHandler(const std::shared_ptr<NetHandler> & handler) {
        handler_ = handler;
    }

    PacketStream stream_;

private:
    Connection conn_;
    std::string username_;

    std::thread netThread_;
    std::queue<ClientBoundPacket> packetQueue_;
    std::mutex queueMutex_;
    std::atomic<bool> running_;

    std::shared_ptr<NetHandler> handler_;

    void networkLoop();

};
