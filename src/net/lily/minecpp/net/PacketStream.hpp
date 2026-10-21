#pragma once
#include "Connection.hpp"
#include <vector>
#include <cstdint>
#include <mutex>

typedef struct evp_cipher_ctx_st EVP_CIPHER_CTX;

class PacketStream {
public:
    explicit PacketStream(Connection& conn);
    ~PacketStream();

    static void writeVarInt(uint32_t value, std::vector<uint8_t> &buffer);

    bool sendPacket(uint32_t packetId, const std::vector<uint8_t> &data) const;

    bool receivePacket(uint32_t &packetId, std::vector<uint8_t> &data) const;

    bool readVarInt(uint32_t &value) const;

    static bool readVarInt(const std::vector<uint8_t> &buffer, size_t &offset, uint32_t &value);

    void setCompression(uint32_t threshold);
    bool enableEncryption(const std::vector<uint8_t>& sharedSecret);

private:
    Connection& conn_;

    mutable std::vector<uint8_t> recvBuffer_;
    mutable size_t recvOffset_ = 0;

    bool compressionEnabled_ = false;
    uint32_t compressionThreshold_ = 0;
    mutable EVP_CIPHER_CTX* encryptContext_ = nullptr;
    mutable EVP_CIPHER_CTX* decryptContext_ = nullptr;
    mutable std::mutex sendMutex_;

    bool sendBytes(const std::vector<uint8_t>& bytes) const;
};
