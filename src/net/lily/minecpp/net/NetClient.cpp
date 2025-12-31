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

bool NetClient::connect(Minecraft* mc) {
    if (!conn_.connect()) return false;
    running_ = true;
    netThread_ = std::thread(&NetClient::networkLoop, this);

    sendHandshake();
    sendLoginStart();

    // std::cout << "[NetClient] Connected to server (network thread started)\n";

    const auto loginHandler = std::make_shared<NetHandlerLogin>(this, "lily", mc);
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

bool NetClient::sendPacket(const ServerBoundPacket &packet) const {
    const auto serialized = packet.serialize();
    // printf("Sending packet with id: %d, length: %lu\n", packet.id, serialized.size());
    return stream_.sendPacket(packet.id, serialized);
}

void NetClient::networkLoop() const {
    while (running_) {
        if (ClientBoundPacket packet{0}; stream_.receivePacket(packet.id, packet.data)) {

            handler_->handlePacket(packet);

            // Debug log
            // std::cout << "[NetClient] Received packet S" << Math::toHexString(packet.id, true)
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