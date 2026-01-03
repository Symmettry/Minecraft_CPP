#pragma once
#include "Packet.hpp"

class ClientBoundPacket : public Packet {
public:
    using Packet::Packet;

    std::vector<uint8_t> data;

    explicit ClientBoundPacket(const uint32_t id, std::vector<uint8_t> data)
        : Packet(id), data(std::move(data)) {}

    static ClientBoundPacket deserialize(const std::vector<uint8_t>& data);

    static void confirm(const std::vector<uint8_t>& data, const size_t& offset) {
        if (data.size() != offset)
            throw std::runtime_error("Unexpected trailing bytes found in function: Expected " + std::to_string(data.size()) + " but found " + std::to_string(offset));
    }

};
