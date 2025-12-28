#include "NetClient.hpp"
#include <iostream>
#include <utility>

#include "packets/handshake/C00Handshake.hpp"
#include "packets/login/NetHandlerLogin.hpp"

NetClient::NetClient(const std::string& host, const uint16_t port, std::string  username)
    : stream_(conn_), conn_(host, port), username_(std::move(username)), running_(false) {}

NetClient::~NetClient() {
    disconnect();
}

bool NetClient::connect() {
    if (!conn_.connect()) return false;
    running_ = true;
    netThread_ = std::thread(&NetClient::networkLoop, this);

    sendHandshake();
    sendLoginStart();

    // std::cout << "[NetClient] Connected to server (network thread started)\n";

    const auto loginHandler = std::make_shared<NetHandlerLogin>(this, "lily");
    handler_ = loginHandler;

    return true;
}

void NetClient::sendHandshake() const {
    sendPacket(C00Handshake{
        47, host(), port(), ConnectionState::LOGIN
    });
}

void NetClient::sendLoginStart() const {
    sendPacket(C00PacketLoginStart{ username_ });
}

void NetClient::disconnect() {
    running_ = false;
    if (netThread_.joinable()) netThread_.join();
    conn_.disconnect();
}

bool NetClient::pollPacket(ClientBoundPacket& packet) {
    std::lock_guard lock(queueMutex_);
    if (packetQueue_.empty()) return false;
    packet = packetQueue_.front();
    packetQueue_.pop();

    // std::cout << "[NetClient] Polling packet ID: " << packet.id
    //                   << ", length: " << packet.data.size() << "\n";

    return true;
}

bool NetClient::sendPacket(const ServerBoundPacket &packet) const {
    const auto serialized = packet.serialize();
    // printf("Sending packet with id: %d, length: %lu\n", packet.id, serialized.size());
    return stream_.sendPacket(packet.id, serialized);
}

void NetClient::networkLoop() {
    while (running_) {
        if (ClientBoundPacket packet{0}; stream_.receivePacket(packet.id, packet.data)) {

            if (handler_->preManage(packet)) continue;

            std::lock_guard lock(queueMutex_);
            packetQueue_.push(packet);

            // Debug log
            // std::cout << "[NetClient] Received packet ID: " << packet.id
            //           << ", length: " << packet.data.size() << "\n";
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // prevent busy loop
        }
    }
}

std::string NetClient::host() const {
    return conn_.host_;
}
uint16_t NetClient::port() const {
    return conn_.port_;
}

void NetClient::tick() {
    if (!handler_) {
        return;
    }

    ClientBoundPacket packet{0};
    while (pollPacket(packet)) {
        handler_->handlePacket(packet);
    }
}
