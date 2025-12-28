#pragma once
#include "Connection.hpp"
#include <vector>
#include <cstdint>

class PacketStream {
public:
    explicit PacketStream(Connection& conn);

    static void writeVarInt(uint32_t value, std::vector<uint8_t> &buffer);

    bool sendPacket(uint32_t packetId, const std::vector<uint8_t> &data) const;

    bool receivePacket(uint32_t &packetId, std::vector<uint8_t> &data) const;

    bool readVarInt(uint32_t &value) const;

    static bool readVarInt(const std::vector<uint8_t> &buffer, size_t &offset, uint32_t &value);

    void setCompression(uint32_t threshold);

private:
    Connection& conn_;

    bool compressionEnabled_ = false;
    uint32_t compressionThreshold_ = 0;
};
