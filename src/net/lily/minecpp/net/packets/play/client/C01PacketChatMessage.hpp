#pragma once
#include <string>
#include <vector>
#include "../../Packet.hpp"
#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"

class C01PacketChatMessage : public ServerBoundPacket {
public:
    std::string message;

    C01PacketChatMessage() : ServerBoundPacket(0x01) {}
    explicit C01PacketChatMessage(const std::string& msg) 
        : ServerBoundPacket(0x01), message(msg.substr(0, 100)) {}

    [[nodiscard]] std::vector<uint8_t> serialize() const override {
        std::vector<uint8_t> buffer;
        writeString(message, buffer);
        return buffer;
    }
};
