#include "Connection.hpp"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib") // link Winsock library
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <utility>

Connection::Connection(std::string host, const uint16_t port)
    : host_(std::move(host)), port_(port), sockfd_(-1) {}

Connection::~Connection() { disconnect(); }

bool Connection::connect() {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* addresses = nullptr;
    const std::string port = std::to_string(port_);
    if (getaddrinfo(host_.c_str(), port.c_str(), &hints, &addresses) != 0) {
        std::cerr << "[Connection] Unable to resolve " << host_ << '\n';
        return false;
    }

    for (const addrinfo* address = addresses; address != nullptr; address = address->ai_next) {
        sockfd_ = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (sockfd_ < 0) continue;
        if (::connect(sockfd_, address->ai_addr, address->ai_addrlen) == 0) break;
        disconnect();
    }
    freeaddrinfo(addresses);

    if (sockfd_ < 0) {
        std::cerr << "[Connection] Connection to " << host_ << ':' << port_
                  << " failed: " << std::strerror(errno) << '\n';
        return false;
    }
    return true;
}

void Connection::disconnect() {
    if (sockfd_ >= 0) {
#ifdef _WIN32
        closesocket(sockfd_);
#else
        close(sockfd_);
#endif
        sockfd_ = -1;
    }
}

bool Connection::isConnected() const { return sockfd_ >= 0; }

bool Connection::sendBytes(const std::vector<uint8_t>& data) const {
    if (!isConnected()) return false;
    size_t offset = 0;
    while (offset < data.size()) {
#ifdef _WIN32
        const int sent = send(sockfd_, reinterpret_cast<const char*>(data.data() + offset),
            static_cast<int>(data.size() - offset), 0);
        if (sent == SOCKET_ERROR || sent == 0) return false;
#else
        const ssize_t sent = send(sockfd_, data.data() + offset, data.size() - offset, MSG_NOSIGNAL);
        if (sent <= 0) return false;
#endif
        offset += static_cast<size_t>(sent);
    }
    return true;
}

bool Connection::receiveBytes(uint8_t* buffer, const size_t length) const {
    if (!isConnected()) return false;

    size_t total = 0;
    while (total < length) {
        const ssize_t recvd = recv(sockfd_,
            #ifdef _WIN32
                reinterpret_cast<char *>(buffer + total)
            #else
                buffer + total
            #endif
                , length - total, MSG_WAITALL);
        if (recvd <= 0) return false;
        total += static_cast<size_t>(recvd);
    }

    return true;
}
