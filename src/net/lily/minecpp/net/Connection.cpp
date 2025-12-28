#include "Connection.hpp"
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <utility>

Connection::Connection(std::string host, const uint16_t port)
    : host_(std::move(host)), port_(port), sockfd_(-1) {}

Connection::~Connection() { disconnect(); }

bool Connection::connect() {
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ < 0) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    if (inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) <= 0) return false;

    if (::connect(sockfd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) return false;
    return true;
}

void Connection::disconnect() {
    if (sockfd_ >= 0) {
        close(sockfd_);
        sockfd_ = -1;
    }
}

bool Connection::isConnected() const { return sockfd_ >= 0; }

bool Connection::sendBytes(const std::vector<uint8_t>& data) const {
    if (!isConnected()) return false;
    const ssize_t sent = ::send(sockfd_, data.data(), data.size(), 0);
    return sent == static_cast<ssize_t>(data.size());
}

bool Connection::receiveBytes(uint8_t* buffer, const size_t length) const {
    if (!isConnected()) return false;

    size_t total = 0;
    while (total < length) {
        const ssize_t recvd = recv(sockfd_, buffer + total, length - total, MSG_WAITALL);
        if (recvd <= 0) return false;
        total += static_cast<size_t>(recvd);
    }

    return true;
}
