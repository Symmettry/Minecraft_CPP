#pragma once
#include <string>

#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"
#include "net/lily/minecpp/net/packets/login/client/C01PacketEncryptionResponse.hpp"

enum class ConnectionState : uint32_t {
    HANDSHAKE = 0,
    STATUS = 1,
    LOGIN = 2,
    PLAY = 3
};

class C00Handshake final : public ServerBoundPacket {
public:
    int protocolVersion;
    std::string ip;
    uint16_t port;
    ConnectionState requestedState;
    bool hasFMLMarker = false;

    explicit C00Handshake(const int version, const std::string& address, const uint16_t port, const ConnectionState state)
        : ServerBoundPacket(0x00), protocolVersion(version), ip(address), port(port), requestedState(state) {
    }

    [[nodiscard]] PacketBuffer serialize() const override {
        buf.writeVarInt(protocolVersion);

        const std::string ipWithMarker = ip + "\0FML\0";
        buf.writeString(ipWithMarker);

        buf.writeUShort(port);

        buf.writeVarInt(static_cast<uint32_t>(requestedState));

        return buf;
    }
};
