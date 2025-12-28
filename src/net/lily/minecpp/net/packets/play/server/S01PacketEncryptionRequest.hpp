#pragma once
#include <vector>
#include <string>
#include <stdexcept>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include "../../Packet.hpp"
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"

class S01PacketEncryptionRequest : public ClientBoundPacket {
public:
    std::string serverId;
    std::vector<uint8_t> publicKeyEncoded;
    std::vector<uint8_t> verifyToken;

    S01PacketEncryptionRequest() : ClientBoundPacket(0x01) {}

    S01PacketEncryptionRequest(const std::string& serverId, const std::vector<uint8_t>& pubKey, const std::vector<uint8_t>& token)
        : ClientBoundPacket(0x01), serverId(serverId), publicKeyEncoded(pubKey), verifyToken(token) {}

    static S01PacketEncryptionRequest deserialize(const std::vector<uint8_t>& data) {
        S01PacketEncryptionRequest packet;
        size_t offset = 0;
        packet.serverId = readString(data, offset, 20);
        packet.publicKeyEncoded = readByteArray(data, offset);
        packet.verifyToken = readByteArray(data, offset);
        return packet;
    }

    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> getPublicKey() const {
        const unsigned char* p = publicKeyEncoded.data();
        EVP_PKEY* key = d2i_PUBKEY(nullptr, &p, publicKeyEncoded.size());
        if (!key) throw std::runtime_error("Failed to decode public key");
        return std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>(key, &EVP_PKEY_free);
    }
};
