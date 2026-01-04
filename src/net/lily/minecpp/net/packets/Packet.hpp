#pragma once

#include <cstdint>

class Packet {
public:
    uint32_t id = 0;

    explicit Packet(const uint32_t id) : id(id) {}
    virtual ~Packet() = default;

};
