#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
#include "../../Packet.hpp"
#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"
#include "net/lily/minecpp/net/packets/handshake/C00Handshake.hpp"
#include "net/lily/minecpp/net/packets/login/client/C01PacketEncryptionResponse.hpp"

class C17PacketCustomPayload final : public ServerBoundPacket {
public:
    std::string channel;
    std::vector<uint8_t> data;

    C17PacketCustomPayload(std::string channel, std::vector<uint8_t> data)
        : ServerBoundPacket(0x17), channel(std::move(channel)), data(std::move(data)) {
        if (this->data.size() > 32767) {
            throw std::runtime_error("Payload may not be larger than 32767 bytes");
        }
    }

    C17PacketCustomPayload(std::string channel, const std::string& str)
    : ServerBoundPacket(0x17), channel(std::move(channel)) {
        buf.writeString(str);
        if (data.size() > 32767) {
            throw std::runtime_error("Payload may not be larger than 32767 bytes");
        }
    }

    PacketBuffer serialize() const override {
        std::vector<uint8_t> buffer;
        buf.writeString(channel);
        if (data.size() > 32767) throw std::runtime_error("Payload may not be larger than 32767 bytes");
        buffer.insert(buffer.end(), data.begin(), data.end());
        return buf;
    }
};
