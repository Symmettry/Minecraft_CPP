#pragma once
#include <string>
#include <vector>
#include "../../Packet.hpp"
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

class S00PacketDisconnect final : public ClientBoundPacket {
public:
    std::string reason;

    S00PacketDisconnect() : ClientBoundPacket(0x00) {}

    explicit S00PacketDisconnect(const std::string& reasonIn)
        : ClientBoundPacket(0x00), reason(reasonIn) {}

    static S00PacketDisconnect deserialize(const PacketBuffer& buf) {
        S00PacketDisconnect packet;
        packet.reason = buf.readString();
        return packet;
    }

};
