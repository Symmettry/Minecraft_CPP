#pragma once
#include <string>
#include <vector>
#include "../../Packet.hpp"

class S02PacketLoginSuccess : public ClientBoundPacket {
public:
    std::string uuid;
    std::string username;

    S02PacketLoginSuccess() : ClientBoundPacket(0x02) {}

    // Deserialize received packet from server
    static S02PacketLoginSuccess deserialize(const std::vector<uint8_t>& data) {
        S02PacketLoginSuccess packet;
        size_t offset = 0;

        packet.uuid = readString(data, offset, 36);
        packet.username = readString(data, offset, 16);

        return packet;
    }

};
