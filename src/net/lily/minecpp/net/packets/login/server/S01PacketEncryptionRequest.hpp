#pragma once
#include <string>
#include <vector>
#include "../../Packet.hpp"

class S01PacketEncryptionRequest : public ClientBoundPacket {
public:
    std::string hashedServerId;
    std::vector<uint8_t> publicKeyEncoded;
    std::vector<uint8_t> verifyToken;

    S01PacketEncryptionRequest() : ClientBoundPacket(0x01) {}

    static S01PacketEncryptionRequest deserialize(const std::vector<uint8_t>& data) {
        S01PacketEncryptionRequest packet;
        size_t offset = 0;

        packet.hashedServerId = readString(data, offset, 20);
        packet.publicKeyEncoded = readByteArray(data, offset);
        packet.verifyToken = readByteArray(data, offset);

        return packet;
    }

};
