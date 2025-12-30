#define LONG static_cast<long>

#include "PacketStream.hpp"
#include <vector>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <zlib.h>

#include "net/lily/minecpp/util/Math.hpp"

PacketStream::PacketStream(Connection& conn) : conn_(conn) {}

void PacketStream::writeVarInt(uint32_t value, std::vector<uint8_t>& buffer) {
    do {
        uint8_t temp = value & 0x7F;
        value >>= 7;
        if (value != 0) temp |= 0x80;
        buffer.push_back(temp);
    } while (value != 0);
}

bool PacketStream::sendPacket(const uint32_t packetId, const std::vector<uint8_t> &data) const {
    printf("[NetClient] Sent packet C%s\n", Math::toHexString(packetId, true).c_str());

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

        return conn_.sendBytes(totalPacket);
    }
    if (compressionEnabled_) {
        std::vector<uint8_t> lengthPrefix;
        writeVarInt(0, lengthPrefix);

        std::vector<uint8_t> totalPacket;
        writeVarInt(static_cast<uint32_t>(lengthPrefix.size() + payload.size()), totalPacket);
        totalPacket.insert(totalPacket.end(), lengthPrefix.begin(), lengthPrefix.end());
        totalPacket.insert(totalPacket.end(), payload.begin(), payload.end());

        return conn_.sendBytes(totalPacket);
    }
    std::vector<uint8_t> totalPacket;
    writeVarInt(static_cast<uint32_t>(payload.size()), totalPacket);
    totalPacket.insert(totalPacket.end(), payload.begin(), payload.end());
    return conn_.sendBytes(totalPacket);
}

bool PacketStream::receivePacket(uint32_t& packetId, std::vector<uint8_t>& data) const {
    static std::vector<uint8_t> recvBuffer;

    if (!conn_.isConnected()) {
        std::cerr << "Error reading packet; not connected to server" << std::endl;
        return false;
    }

    uint8_t tmp[1048576];
    const ssize_t recvd = ::recv(conn_.sockfd_, tmp, sizeof(tmp), MSG_DONTWAIT);
    if (recvd > 0) {
        recvBuffer.insert(recvBuffer.end(), tmp, tmp + recvd);
    }

    uint32_t packetLength = 0;
    size_t offset = 0;
    if (!readVarInt(recvBuffer, offset, packetLength)) {
        // not enough data yet, wait for more
        return false;
    }
    if (recvBuffer.size() < offset + packetLength) {
        // packet incomplete, wait for more data
        return false;
    }

    const size_t packetStart = offset;
    size_t payloadOffset = offset;

    if (compressionEnabled_) {
        uint32_t dataLength = 0;
        if (!readVarInt(recvBuffer, payloadOffset, dataLength)) {
            std::cerr << "Error reading packet; buffer size issue" << std::endl;
            return false;
        }

        const size_t compressedSize = packetLength - (payloadOffset - packetStart);

        if (dataLength != 0) {
            data.resize(dataLength);

            z_stream zs{};
            zs.next_in = recvBuffer.data() + payloadOffset;
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
                recvBuffer.begin() + payloadOffset,
                recvBuffer.begin() + payloadOffset + compressedSize
            );
        }
    } else {
        data.assign(
            recvBuffer.begin() + payloadOffset,
            recvBuffer.begin() + payloadOffset + packetLength
        );
    }

    size_t dataOffset = 0;
    if (!readVarInt(data, dataOffset, packetId)) {
        std::cerr << "Error reading packet; buffer size issue" << std::endl;
        return false;
    }

    data.erase(data.begin(), data.begin() + dataOffset);

    recvBuffer.erase(
        recvBuffer.begin(),
        recvBuffer.begin() + packetStart + packetLength
    );

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

        uint8_t byte = buffer[offset++];
        value |= (byte & 0x7F) << shift;

        if (!(byte & 0x80)) break;

        shift += 7;
        count++;
        if (count > 5) return false; // VarInt too big
    }

    return true;
}

void PacketStream::setCompression(const uint32_t threshold) {
    compressionEnabled_ = true;
    compressionThreshold_ = threshold;
    printf("[setCompression] this=%p, compressionThreshold_=%d\n", this, compressionThreshold_);
}
