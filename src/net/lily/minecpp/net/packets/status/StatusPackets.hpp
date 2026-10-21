#pragma once

#include <cstdint>
#include <string>

#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"
#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"

class StatusRequestPacket final : public ServerBoundPacket {
public:
    StatusRequestPacket() : ServerBoundPacket(0x00) {}
    [[nodiscard]] PacketBuffer serialize() const override { return {}; }
};

class StatusPingPacket final : public ServerBoundPacket {
public:
    explicit StatusPingPacket(const uint64_t payload = 0) : ServerBoundPacket(0x01), payload(payload) {}
    [[nodiscard]] PacketBuffer serialize() const override {
        PacketBuffer result;
        for (int shift = 56; shift >= 0; shift -= 8) result.writeByte(this->payload >> shift & 0xFF);
        return result;
    }
    uint64_t payload;
};

class StatusResponsePacket final : public ClientBoundPacket {
public:
    StatusResponsePacket() : ClientBoundPacket(0x00) {}
    static StatusResponsePacket deserialize(const PacketBuffer& buf) {
        StatusResponsePacket packet;
        packet.json = buf.readString();
        return packet;
    }
    std::string json;
};

class StatusPongPacket final : public ClientBoundPacket {
public:
    StatusPongPacket() : ClientBoundPacket(0x01) {}
    static StatusPongPacket deserialize(const PacketBuffer& buf) {
        StatusPongPacket packet;
        packet.payload = buf.readULong();
        return packet;
    }
    uint64_t payload = 0;
};
