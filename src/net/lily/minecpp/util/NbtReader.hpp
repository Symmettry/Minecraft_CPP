#pragma once

#include <cstdint>
#include <string>

#include "net/lily/minecpp/net/packets/play/server/EntityMetadata.hpp"

namespace NbtReader {
template<typename Visitor>
void visitRootCompound(const PacketBuffer& buffer, Visitor&& visitor) {
    const uint8_t rootType = buffer.readByte();
    if (rootType == 0) return;
    EntityMetadataDetail::skipNbtString(buffer);
    if (rootType != 10) {
        EntityMetadataDetail::skipNbtPayload(buffer, rootType);
        return;
    }
    while (true) {
        const uint8_t type = buffer.readByte();
        if (type == 0) return;
        const uint16_t nameLength = static_cast<uint16_t>(buffer.readUShort());
        buffer.require("NBT tag name", nameLength);
        const std::string name(buffer.buffer.begin() + buffer.offset,
            buffer.buffer.begin() + buffer.offset + nameLength);
        buffer.offset += nameLength;
        if (!visitor(type, name, buffer)) EntityMetadataDetail::skipNbtPayload(buffer, type);
    }
}
}
