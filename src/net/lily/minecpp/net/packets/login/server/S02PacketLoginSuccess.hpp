#pragma once
#include <string>

class S02PacketLoginSuccess final : public ClientBoundPacket {
public:
    std::string uuid;
    std::string username;

    S02PacketLoginSuccess() : ClientBoundPacket(0x02) {}

    static S02PacketLoginSuccess deserialize(const PacketBuffer& buf) {
        S02PacketLoginSuccess packet;

        packet.uuid = buf.readString(36);
        packet.username = buf.readString(16);

        return packet;
    }

};
