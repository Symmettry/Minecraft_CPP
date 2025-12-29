#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
#include "../../Packet.hpp"
#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"

class C17PacketCustomPayload : public ServerBoundPacket {
public:
    std::string channel;
    std::vector<uint8_t> data;

    C17PacketCustomPayload() : ServerBoundPacket(0x17) {}

    C17PacketCustomPayload(std::string channel, std::vector<uint8_t> data)
        : ServerBoundPacket(0x17), channel(std::move(channel)), data(std::move(data)) {
        if (this->data.size() > 32767) {
            throw std::runtime_error("Payload may not be larger than 32767 bytes");
        }
    }

    C17PacketCustomPayload(std::string channel, const std::string& str)
    : ServerBoundPacket(0x17), channel(std::move(channel)) {
        Packet::writeString(str, data);
        if (data.size() > 32767) {
            throw std::runtime_error("Payload may not be larger than 32767 bytes");
        }
    }

    std::vector<uint8_t> serialize() const override {
        std::vector<uint8_t> buffer;
        writeString(channel, buffer);
        if (data.size() > 32767) throw std::runtime_error("Payload may not be larger than 32767 bytes");
        buffer.insert(buffer.end(), data.begin(), data.end());
        return buffer;
    }
};
