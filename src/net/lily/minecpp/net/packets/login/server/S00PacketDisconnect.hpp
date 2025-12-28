#pragma once
#include <string>
#include <vector>
#include "../../Packet.hpp"
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

class S00PacketDisconnect : public ClientBoundPacket {
public:
    std::string reason;

    S00PacketDisconnect() : ClientBoundPacket(0x00) {}

    explicit S00PacketDisconnect(const std::string& reasonIn)
        : ClientBoundPacket(0x00), reason(reasonIn) {}

    static S00PacketDisconnect deserialize(const std::vector<uint8_t>& data) {
        S00PacketDisconnect packet;
        size_t offset = 0;
        packet.reason = readString(data, offset);
        return packet;
    }

};
