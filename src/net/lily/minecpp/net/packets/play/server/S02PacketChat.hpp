#pragma once
#include <string>
#include <vector>
#include "../../Packet.hpp"
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

class S02PacketChat : public ClientBoundPacket {
public:
    std::string chatMessage;
    uint8_t type = 1;

    S02PacketChat() : ClientBoundPacket(0x02) {}
    explicit S02PacketChat(const std::string& msg, uint8_t typeIn = 1)
        : ClientBoundPacket(0x02), chatMessage(msg), type(typeIn) {}

    static S02PacketChat deserialize(const std::vector<uint8_t>& data) {
        S02PacketChat packet;
        size_t offset = 0;
        packet.chatMessage = readString(data, offset);
        if (offset < data.size()) {
            packet.type = data[offset];
        }
        return packet;
    }
};
