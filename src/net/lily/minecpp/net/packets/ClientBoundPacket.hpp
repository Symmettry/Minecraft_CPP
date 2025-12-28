#pragma once
#include "Packet.hpp"

class ClientBoundPacket : public Packet {
public:
    using Packet::Packet;

    std::vector<uint8_t> data;

    explicit ClientBoundPacket(const uint32_t id, std::vector<uint8_t> data)
        : Packet(id), data(std::move(data)) {}

    static ClientBoundPacket deserialize(const std::vector<uint8_t>& data);
};
