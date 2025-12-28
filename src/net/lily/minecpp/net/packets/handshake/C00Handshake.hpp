#pragma once
#include <string>
#include <vector>

#include "../Packet.hpp"
#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"

enum class ConnectionState : uint32_t {
    HANDSHAKE = 0,
    STATUS = 1,
    LOGIN = 2,
    PLAY = 3
};

class C00Handshake : public ServerBoundPacket {
public:
    int protocolVersion;
    std::string ip;
    uint16_t port;
    ConnectionState requestedState;
    bool hasFMLMarker = false;

    explicit C00Handshake(const int version, const std::string& address, const uint16_t port, const ConnectionState state)
        : ServerBoundPacket(0x00), protocolVersion(version), ip(address), port(port), requestedState(state) {
    }

    [[nodiscard]] std::vector<uint8_t> serialize() const override {
        std::vector<uint8_t> buffer;

        writeVarInt(protocolVersion, buffer);

        std::string ipWithMarker = ip + "\0FML\0";
        writeString(ipWithMarker, buffer);

        buffer.push_back(port >> 8);
        buffer.push_back(port & 0xFF);

        writeVarInt(static_cast<uint32_t>(requestedState), buffer);

        return buffer;
    }
};
