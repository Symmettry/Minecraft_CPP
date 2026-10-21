#pragma once

#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"
#include "net/lily/minecpp/net/packets/play/server/EntityMetadata.hpp"

class C0EPacketClickWindow final : public ServerBoundPacket {
public:
    C0EPacketClickWindow(const uint8_t windowId, const int16_t slot, const uint8_t button,
                        const int16_t actionNumber, const uint8_t mode,
                        const NetworkItemStack& clickedItem)
        : ServerBoundPacket(0x0E), windowId(windowId), slot(slot), button(button),
          actionNumber(actionNumber), mode(mode), clickedItem(clickedItem) {}

    [[nodiscard]] PacketBuffer serialize() const override {
        buf.writeByte(windowId);
        buf.writeUShort(static_cast<uint16_t>(slot));
        buf.writeByte(button);
        buf.writeUShort(static_cast<uint16_t>(actionNumber));
        buf.writeByte(mode);
        EntityMetadataDetail::writeItemStack(buf, clickedItem);
        return buf;
    }

private:
    uint8_t windowId;
    int16_t slot;
    uint8_t button;
    int16_t actionNumber;
    uint8_t mode;
    NetworkItemStack clickedItem;
};
