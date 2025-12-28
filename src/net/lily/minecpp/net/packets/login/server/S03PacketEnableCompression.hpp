#pragma once
#include <vector>
#include "../../Packet.hpp"

class S03PacketEnableCompression : public ClientBoundPacket {
public:
    int compressionThreshold = 0;

    S03PacketEnableCompression() : ClientBoundPacket(0x03) {}

    static S03PacketEnableCompression deserialize(const std::vector<uint8_t>& data) {
        S03PacketEnableCompression packet;
        if (data.empty()) return packet;
        size_t offset = 0;
        packet.compressionThreshold = static_cast<int>(readVarInt(data, offset));
        return packet;
    }

};
