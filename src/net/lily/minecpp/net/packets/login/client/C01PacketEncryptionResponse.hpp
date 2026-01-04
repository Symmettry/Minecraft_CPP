#pragma once
#include <vector>
#include <cstdint>
#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"

class C01PacketEncryptionResponse final : public ServerBoundPacket {
public:
    std::vector<uint8_t> secretKeyEncrypted;
    std::vector<uint8_t> verifyTokenEncrypted;

    C01PacketEncryptionResponse() : ServerBoundPacket(0x01) {}

    C01PacketEncryptionResponse(const std::vector<uint8_t>& encryptedSecretKey,
                                const std::vector<uint8_t>& encryptedVerifyToken)
        : ServerBoundPacket(0x01), secretKeyEncrypted(encryptedSecretKey), verifyTokenEncrypted(encryptedVerifyToken) {}

    PacketBuffer serialize() const override {
        buf.writeByteArray(secretKeyEncrypted);
        buf.writeByteArray(verifyTokenEncrypted);

        return buf;
    }
};
