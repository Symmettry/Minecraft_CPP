#pragma once
#include <string>
#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"

class C01PacketChatMessage final : public ServerBoundPacket {
public:
    std::string message;

    C01PacketChatMessage() : ServerBoundPacket(0x01) {}
    explicit C01PacketChatMessage(const std::string& msg) 
        : ServerBoundPacket(0x01), message(msg.substr(0, 100)) {}

    [[nodiscard]] PacketBuffer serialize() const override {
        buf.writeString(message);
        return buf;
    }
};
