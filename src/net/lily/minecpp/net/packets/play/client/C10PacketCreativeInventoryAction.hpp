#pragma once

#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"
#include "net/lily/minecpp/net/packets/play/server/EntityMetadata.hpp"

// sets a slot's contents in creative mode (the server trusts the client)
class C10PacketCreativeInventoryAction final : public ServerBoundPacket {
public:
    C10PacketCreativeInventoryAction(const int16_t slot, const NetworkItemStack& stack)
        : ServerBoundPacket(0x10), slot(slot), stack(stack) {}

    [[nodiscard]] PacketBuffer serialize() const override {
        buf.writeUShort(static_cast<uint16_t>(slot));
        EntityMetadataDetail::writeItemStack(buf, stack);
        return buf;
    }

private:
    int16_t slot;
    NetworkItemStack stack;
};
