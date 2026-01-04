#pragma once
#include <vector>
#include "../../Packet.hpp"

class S03PacketEnableCompression final : public ClientBoundPacket {
public:
    int compressionThreshold = 0;

    S03PacketEnableCompression() : ClientBoundPacket(0x03) {}

    static S03PacketEnableCompression deserialize(const PacketBuffer& buf) {
        S03PacketEnableCompression packet;
        packet.compressionThreshold = buf.readVarInt();
        return packet;
    }

};
