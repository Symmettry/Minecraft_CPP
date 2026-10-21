#pragma once
#include "PacketStream.hpp"
#include "packets/Packet.hpp"
#include <string>
#include <thread>
#include <queue>
#include <mutex>
#include <atomic>
#include <memory>
#include <optional>

#include "AuthSession.hpp"
#include "NetHandler.hpp"
#include "packets/ServerBoundPacket.hpp"

struct NetClient {
    NetClient(const std::string& host, uint16_t port, std::string  username);
    ~NetClient();

    bool connect(Minecraft *mc);

    bool sendHandshake() const;

    bool sendLoginStart() const;

    void disconnect();

    bool sendPacket(const ServerBoundPacket &packet) const;

    bool handleEncryptionRequest(const std::string& serverId,
        const std::vector<uint8_t>& publicKey, const std::vector<uint8_t>& verifyToken);

    void fail(std::string reason);
    [[nodiscard]] bool hasFailed() const;
    [[nodiscard]] std::string failureReason() const;

    [[nodiscard]] std::string host() const;
    [[nodiscard]] uint16_t port() const;

    void setHandler(const std::shared_ptr<NetHandler> & handler) {
        std::lock_guard lock(handlerMutex_);
        handler_ = handler;
    }

    bool isHandlerDone() const {
        const std::shared_ptr<NetHandler> handler = currentHandler();
        return handler != nullptr && handler->isDone();
    }

private:
    Connection conn_;

public:
    PacketStream stream_;

private:
    std::string username_;
    std::optional<AuthSession> authSession_;

    std::thread netThread_;
    std::mutex queueMutex_;
    std::atomic<bool> running_;
    mutable std::mutex failureMutex_;
    std::string failureReason_;

    std::shared_ptr<NetHandler> handler_;
    mutable std::mutex handlerMutex_;

    void networkLoop();
    [[nodiscard]] std::shared_ptr<NetHandler> currentHandler() const {
        std::lock_guard lock(handlerMutex_);
        return handler_;
    }

};
