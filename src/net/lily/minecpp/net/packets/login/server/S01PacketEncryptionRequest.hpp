#pragma once
#include <string>
#include <vector>

class S01PacketEncryptionRequest final : public ClientBoundPacket {
public:
    std::string hashedServerId;
    std::vector<uint8_t> publicKeyEncoded;
    std::vector<uint8_t> verifyToken;

    S01PacketEncryptionRequest() : ClientBoundPacket(0x01) {}

    static S01PacketEncryptionRequest deserialize(const PacketBuffer& buf) {
        S01PacketEncryptionRequest packet;

        packet.hashedServerId = buf.readString(20);
        packet.publicKeyEncoded = buf.readByteArray();
        packet.verifyToken = buf.readByteArray();

        return packet;
    }

};
