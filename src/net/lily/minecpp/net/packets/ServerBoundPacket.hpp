#pragma once
#include "Packet.hpp"

class ServerBoundPacket : public Packet {
public:
    using Packet::Packet;

    [[nodiscard]] virtual std::vector<uint8_t> serialize() const = 0;
};
