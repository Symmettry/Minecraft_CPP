#pragma once
#include "Packet.hpp"
#include "PacketBuffer.hpp"

class ServerBoundPacket : public Packet {
public:
    using Packet::Packet;

    PacketBuffer buf{};

    [[nodiscard]] virtual PacketBuffer serialize() const = 0;
};
