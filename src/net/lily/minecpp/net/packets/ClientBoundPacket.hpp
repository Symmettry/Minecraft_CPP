#pragma once
#include "Packet.hpp"
#include "PacketBuffer.hpp"

class ClientBoundPacket : public Packet {
public:
    using Packet::Packet;

    const PacketBuffer buf;

    explicit ClientBoundPacket(const uint32_t id, PacketBuffer& data)
        : Packet(id), buf(std::move(data)) {}

    explicit ClientBoundPacket() : Packet(0) {}

    static ClientBoundPacket deserialize(const PacketBuffer& buffer);

};
