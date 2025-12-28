#pragma once
#include <string>
#include <vector>
#include <cstdint>

class Connection {
public:
    Connection(std::string  host, uint16_t port);
    ~Connection();

    bool connect();
    void disconnect();
    [[nodiscard]] bool isConnected() const;

    [[nodiscard]] bool sendBytes(const std::vector<uint8_t> &data) const;
    [[nodiscard]] bool receiveBytes(uint8_t *buffer, size_t length) const;

    std::string host_;
    uint16_t port_;
    int sockfd_;
};
