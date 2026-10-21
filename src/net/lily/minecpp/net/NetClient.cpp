#include "NetClient.hpp"
#include <iostream>
#include <utility>
#include <tracy/Tracy.hpp>

#include "OnlineCrypto.hpp"
#include "packets/handshake/C00Handshake.hpp"
#include "packets/login/NetHandlerLogin.hpp"
#include "packets/login/client/C01PacketEncryptionResponse.hpp"

NetClient::NetClient(const std::string& host, const uint16_t port, std::string  username)
    : conn_(host, port), stream_(conn_), username_(std::move(username)), running_(false) {
    authSession_ = AuthSession::load("account.json");
    if (authSession_ && !authSession_->username.empty()) username_ = authSession_->username;
}

NetClient::~NetClient() {
    disconnect();
}

bool NetClient::connect(Minecraft* mc) {
    if (!conn_.connect()) {
        std::cerr << "[NetClient] Failed to connect to " << host() << ':' << port() << '\n';
        return false;
    }

    handler_ = std::make_shared<NetHandlerLogin>(this, username_, mc);
    running_ = true;
    netThread_ = std::thread(&NetClient::networkLoop, this);

    if (!sendHandshake() || !sendLoginStart()) {
        std::cerr << "[NetClient] Connected, but failed to send the login handshake\n";
        disconnect();
        return false;
    }

    std::cout << "[NetClient] Connected to " << host() << ':' << port()
              << " using protocol 47\n";
    return true;
}

bool NetClient::sendHandshake() const {
    return sendPacket(C00Handshake{
        47, host(), port(), ConnectionState::LOGIN
    });
}

bool NetClient::sendLoginStart() const {
    return sendPacket(C00PacketLoginStart{ username_ });
}

void NetClient::disconnect() {
    running_ = false;
    conn_.disconnect();
    if (netThread_.joinable() && netThread_.get_id() != std::this_thread::get_id())
        netThread_.join();
}

bool NetClient::sendPacket(const ServerBoundPacket &packet) const {
    const auto serialized = packet.serialize();
    // printf("Sending packet with id: %d, length: %lu\n", packet.id, serialized.size());
    return stream_.sendPacket(packet.id, serialized.buffer);
}

bool NetClient::handleEncryptionRequest(const std::string& serverId,
        const std::vector<uint8_t>& publicKey, const std::vector<uint8_t>& verifyToken) {
    std::string error;
    std::vector<uint8_t> sharedSecret;
    if (!OnlineCrypto::createSharedSecret(sharedSecret, error)) {
        fail(std::move(error));
        return false;
    }
    if (!authSession_) {
        fail("Online-mode server requires an account.json file");
        return false;
    }
    const std::string hash = OnlineCrypto::serverHash(serverId, sharedSecret, publicKey);
    if (!authSession_->joinServer(hash, error)) {
        fail(std::move(error));
        return false;
    }
    std::vector<uint8_t> encryptedSecret;
    std::vector<uint8_t> encryptedToken;
    if (!OnlineCrypto::encryptRsa(publicKey, sharedSecret, encryptedSecret, error)
        || !OnlineCrypto::encryptRsa(publicKey, verifyToken, encryptedToken, error)) {
        fail(std::move(error));
        return false;
    }
    if (!sendPacket(C01PacketEncryptionResponse(encryptedSecret, encryptedToken))) {
        fail("Unable to send the encrypted login response");
        return false;
    }
    if (!stream_.enableEncryption(sharedSecret)) {
        fail("Unable to enable encrypted network traffic");
        return false;
    }
    return true;
}

void NetClient::fail(std::string reason) {
    running_ = false;
    conn_.disconnect();
    {
        std::lock_guard lock(failureMutex_);
        if (failureReason_.empty()) failureReason_ = std::move(reason);
    }
}

bool NetClient::hasFailed() const {
    std::lock_guard lock(failureMutex_);
    return !failureReason_.empty();
}

std::string NetClient::failureReason() const {
    std::lock_guard lock(failureMutex_);
    return failureReason_;
}

void NetClient::networkLoop() {
    tracy::SetThreadName("Network");
    while (running_) {
        ZoneScopedN("Network receive");
        if (ClientBoundPacket packet; stream_.receivePacket(packet.id, packet.buf.buffer)) {

            try {
                // keep the active handler alive through dispatch. Login success replaces handler_ from inside NetHandlerLogin::handlePacket
                const std::shared_ptr<NetHandler> handler = currentHandler();
                if (handler != nullptr && handler->handlePacket(packet)) {
                    packet.buf.confirm("S" + Math::toHexString(packet.id));
                }
            } catch (const std::exception& error) {
                // a malformed or half-parsed packet must not take the whole client down
                std::fprintf(stderr, "[NetClient] Dropped packet S%s: %s\n",
                    Math::toHexString(packet.id).c_str(), error.what());
            }

            // debug log: std::cout << "[NetClient] Received packet S" << Math::toHexString(packet.id, true) << ", length: " << packet.data.size() << "\n";
        } else {
            if (!conn_.isConnected()) {
                fail("Connection closed by the server");
                break;
            }
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
