#include "PacketStream.hpp"
#include <vector>
#include <cstdint>
#include <cerrno>
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#define MSG_DONTWAIT 0
#else
#include <sys/socket.h>
#endif

#include <zlib.h>
#include <openssl/evp.h>

#include "net/lily/minecpp/util/Math.hpp"

PacketStream::PacketStream(Connection& conn) : conn_(conn) {}

PacketStream::~PacketStream() {
    EVP_CIPHER_CTX_free(encryptContext_);
    EVP_CIPHER_CTX_free(decryptContext_);
}

bool PacketStream::sendBytes(const std::vector<uint8_t>& bytes) const {
    std::lock_guard lock(sendMutex_);
    if (encryptContext_ == nullptr) return conn_.sendBytes(bytes);
    std::vector<uint8_t> encrypted(bytes.size() + EVP_MAX_BLOCK_LENGTH);
    int written = 0;
    if (EVP_EncryptUpdate(encryptContext_, encrypted.data(), &written,
            bytes.data(), static_cast<int>(bytes.size())) != 1) return false;
    encrypted.resize(static_cast<size_t>(written));
    return conn_.sendBytes(encrypted);
}

void PacketStream::writeVarInt(uint32_t value, std::vector<uint8_t>& buffer) {
    do {
        uint8_t temp = value & 0x7F;
        value >>= 7;
        if (value != 0) temp |= 0x80;
        buffer.push_back(temp);
    } while (value != 0);
}

bool PacketStream::sendPacket(const uint32_t packetId, const std::vector<uint8_t> &data) const {
    // printf("[NetClient] Sent packet C%s\n", Math::toHexString(packetId, true).c_str());

    std::vector<uint8_t> payload;
    writeVarInt(packetId, payload);
    payload.insert(payload.end(), data.begin(), data.end());

    std::vector<uint8_t> finalPacket;

    if (compressionEnabled_ && payload.size() >= compressionThreshold_) {
        std::vector<uint8_t> compressedData(compressBound(payload.size()));
        uLongf compressedSize = compressedData.size();
        compress2(compressedData.data(), &compressedSize, payload.data(), payload.size(), Z_BEST_SPEED);
        compressedData.resize(compressedSize);

        std::vector<uint8_t> lengthPrefix;
        writeVarInt(payload.size(), lengthPrefix);

        std::vector<uint8_t> totalPacket;
        writeVarInt(static_cast<uint32_t>(lengthPrefix.size() + compressedData.size()), totalPacket);

        totalPacket.insert(totalPacket.end(), lengthPrefix.begin(), lengthPrefix.end());
        totalPacket.insert(totalPacket.end(), compressedData.begin(), compressedData.end());

        return sendBytes(totalPacket);
    }
    if (compressionEnabled_) {
        std::vector<uint8_t> lengthPrefix;
        writeVarInt(0, lengthPrefix);

        std::vector<uint8_t> totalPacket;
        writeVarInt(static_cast<uint32_t>(lengthPrefix.size() + payload.size()), totalPacket);
        totalPacket.insert(totalPacket.end(), lengthPrefix.begin(), lengthPrefix.end());
        totalPacket.insert(totalPacket.end(), payload.begin(), payload.end());

        return sendBytes(totalPacket);
    }
    std::vector<uint8_t> totalPacket;
    writeVarInt(static_cast<uint32_t>(payload.size()), totalPacket);
    totalPacket.insert(totalPacket.end(), payload.begin(), payload.end());
    return sendBytes(totalPacket);
}

bool PacketStream::receivePacket(uint32_t& packetId, std::vector<uint8_t>& data) const {
    if (!conn_.isConnected()) {
        std::cerr << "Error reading packet; not connected to server" << std::endl;
        return false;
    }

#ifdef _WIN32
    char tmp[65536];
    const ssize_t recvd = recv(conn_.sockfd_, tmp, sizeof(tmp), MSG_DONTWAIT);
    if (recvd == SOCKET_ERROR) {
        const int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) return false; // no data yet
        std::cerr << "recv failed with error: " << err << std::endl;
        conn_.disconnect();
        return false;
    }
#else
    uint8_t tmp[65536];
    const ssize_t recvd = recv(conn_.sockfd_, tmp, sizeof(tmp), MSG_DONTWAIT);
#endif
    if (recvd == 0) {
        conn_.disconnect();
        return false;
    }
#ifndef _WIN32
    if (recvd < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        conn_.disconnect();
        return false;
    }
#endif
    if (recvd > 0) {
        if (decryptContext_ == nullptr) {
            recvBuffer_.insert(recvBuffer_.end(), tmp, tmp + recvd);
        } else {
            std::vector<uint8_t> decrypted(static_cast<size_t>(recvd) + EVP_MAX_BLOCK_LENGTH);
            int written = 0;
            if (EVP_DecryptUpdate(decryptContext_, decrypted.data(), &written,
                    reinterpret_cast<const uint8_t*>(tmp), static_cast<int>(recvd)) != 1) {
                conn_.disconnect();
                return false;
            }
            recvBuffer_.insert(recvBuffer_.end(), decrypted.begin(), decrypted.begin() + written);
        }
    }

    uint32_t packetLength = 0;
    size_t offset = recvOffset_;
    if (!readVarInt(recvBuffer_, offset, packetLength)) {
        // not enough data yet, wait for more
        return false;
    }
    if (recvBuffer_.size() < offset + packetLength) {
        // packet incomplete, wait for more data
        return false;
    }

    const size_t packetStart = offset;
    size_t payloadOffset = offset;

    if (compressionEnabled_) {
        uint32_t dataLength = 0;
        if (!readVarInt(recvBuffer_, payloadOffset, dataLength)) {
            std::cerr << "Error reading packet; buffer size issue" << std::endl;
            return false;
        }

        const size_t compressedSize = packetLength - (payloadOffset - packetStart);

        if (dataLength != 0) {
            data.resize(dataLength);

            z_stream zs{};
            zs.next_in = recvBuffer_.data() + payloadOffset;
            zs.avail_in = static_cast<uInt>(compressedSize);
            zs.next_out = data.data();
            zs.avail_out = dataLength;

            if (inflateInit(&zs) != Z_OK) {
                std::cerr << "inflateInit failed" << std::endl;
                return false;
            }

            int ret;
            do {
                ret = inflate(&zs, Z_NO_FLUSH);
                if (ret != Z_OK && ret != Z_STREAM_END) {
                    std::cerr << "inflate failed: " << ret << std::endl;
                    inflateEnd(&zs);
                    return false;
                }
            } while (ret != Z_STREAM_END);

            inflateEnd(&zs);
        } else {
            data.assign(
                recvBuffer_.begin() + payloadOffset,
                recvBuffer_.begin() + payloadOffset + compressedSize
            );
        }
    } else {
        data.assign(
            recvBuffer_.begin() + payloadOffset,
            recvBuffer_.begin() + payloadOffset + packetLength
        );
    }

    size_t dataOffset = 0;
    if (!readVarInt(data, dataOffset, packetId)) {
        std::cerr << "Error reading packet; buffer size issue" << std::endl;
        return false;
    }

    data.erase(data.begin(), data.begin() + dataOffset);

    recvOffset_ = packetStart + packetLength;
    if (recvOffset_ == recvBuffer_.size()) {
        recvBuffer_.clear();
        recvOffset_ = 0;
    } else if (recvOffset_ >= 65536 && recvOffset_ * 2 >= recvBuffer_.size()) {
        recvBuffer_.erase(recvBuffer_.begin(), recvBuffer_.begin() + recvOffset_);
        recvOffset_ = 0;
    }

    return true;
}

bool PacketStream::readVarInt(uint32_t& value) const {
    value = 0;
    int count = 0;
    uint32_t shift = 0;
    uint8_t byte = 0;

    do {
        if (!conn_.receiveBytes(&byte, 1)) return false;
        value |= (byte & 0x7F) << shift;
        shift += 7;
        count++;
        if (count > 5) return false;
    } while (byte & 0x80);

    return true;
}
bool PacketStream::readVarInt(const std::vector<uint8_t>& buffer, size_t& offset, uint32_t& value) {
    value = 0;
    int shift = 0;
    int count = 0;

    while (true) {
        if (offset >= buffer.size()) return false;

        const uint8_t byte = buffer[offset++];
        value |= (byte & 0x7F) << shift;

        if (!(byte & 0x80)) break;

        shift += 7;
        count++;
        if (count > 5) return false; // varInt too big
    }

    return true;
}

void PacketStream::setCompression(const uint32_t threshold) {
    compressionEnabled_ = true;
    compressionThreshold_ = threshold;
    printf("[setCompression] this=%p, compressionThreshold_=%d\n", this, compressionThreshold_);
}

bool PacketStream::enableEncryption(const std::vector<uint8_t>& sharedSecret) {
    if (sharedSecret.size() != 16 || encryptContext_ != nullptr || decryptContext_ != nullptr)
        return false;
    encryptContext_ = EVP_CIPHER_CTX_new();
    decryptContext_ = EVP_CIPHER_CTX_new();
    if (encryptContext_ == nullptr || decryptContext_ == nullptr
        || EVP_EncryptInit_ex(encryptContext_, EVP_aes_128_cfb8(), nullptr,
            sharedSecret.data(), sharedSecret.data()) != 1
        || EVP_DecryptInit_ex(decryptContext_, EVP_aes_128_cfb8(), nullptr,
            sharedSecret.data(), sharedSecret.data()) != 1) {
        EVP_CIPHER_CTX_free(encryptContext_);
        EVP_CIPHER_CTX_free(decryptContext_);
        encryptContext_ = nullptr;
        decryptContext_ = nullptr;
        return false;
    }
    EVP_CIPHER_CTX_set_padding(encryptContext_, 0);
    EVP_CIPHER_CTX_set_padding(decryptContext_, 0);
    return true;
}
