#pragma once
#include <string>
#include <vector>
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

class S02PacketChat final : public ClientBoundPacket {
public:
    std::string chatMessage;
    uint8_t type = 1;

    S02PacketChat() : ClientBoundPacket(0x02) {}
    explicit S02PacketChat(const std::string& msg, const uint8_t typeIn = 1)
        : ClientBoundPacket(0x02), chatMessage(msg), type(typeIn) {}

    static S02PacketChat deserialize(const PacketBuffer& buf) {
        S02PacketChat packet;
        packet.chatMessage = buf.readString();
        if (buf.offset < buf.size()) {
            packet.type = buf.readByte();
        }
        return packet;
    }
};
