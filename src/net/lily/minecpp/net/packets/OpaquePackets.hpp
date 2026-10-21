#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

#include "ClientBoundPacket.hpp"
#include "ServerBoundPacket.hpp"

template <uint32_t PacketId>
class OpaqueClientBoundPacket : public ClientBoundPacket {
public:
    OpaqueClientBoundPacket() : ClientBoundPacket(PacketId) {}

    explicit OpaqueClientBoundPacket(PacketBuffer payload)
        : ClientBoundPacket(PacketId), payload(std::move(payload)) {}

    static OpaqueClientBoundPacket deserialize(const PacketBuffer& buf) {
        PacketBuffer payload;
        payload.buffer.assign(buf.buffer.begin() + static_cast<std::ptrdiff_t>(buf.offset), buf.buffer.end());
        return OpaqueClientBoundPacket{std::move(payload)};
    }

    PacketBuffer payload;
};

template <uint32_t PacketId>
class OpaqueServerBoundPacket : public ServerBoundPacket {
public:
    OpaqueServerBoundPacket() : ServerBoundPacket(PacketId) {}

    explicit OpaqueServerBoundPacket(PacketBuffer payload)
        : ServerBoundPacket(PacketId), payload(std::move(payload)) {}

    [[nodiscard]] PacketBuffer serialize() const override {
        return this->payload;
    }

    PacketBuffer payload;
};
