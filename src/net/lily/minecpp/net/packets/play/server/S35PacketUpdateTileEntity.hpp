#pragma once

#include <optional>
#include "net/lily/minecpp/util/NbtReader.hpp"
#include "net/lily/minecpp/world/blockentity/BlockEntity.hpp"

struct S35PacketUpdateTileEntity {
    glm::ivec3 position{};
    uint8_t action{};
    std::optional<BlockEntity> blockEntity;

    static S35PacketUpdateTileEntity deserialize(const PacketBuffer& buffer) {
        S35PacketUpdateTileEntity packet;
        packet.position = buffer.readPosition();
        packet.action = buffer.readByte();
        switch (packet.action) {
            case 4: {
                SkullBlockEntity skull;
                NbtReader::visitRootCompound(buffer, [&](const uint8_t type, const std::string& name,
                                                         const PacketBuffer& nbt) {
                    if (type == 1 && name == "SkullType") { skull.type = nbt.readByte(); return true; }
                    if (type == 1 && name == "Rot") { skull.rotation = nbt.readByte() & 15U; return true; }
                    return false;
                });
                packet.blockEntity = BlockEntity{packet.action, skull};
                break;
            }
            default:
                NbtReader::visitRootCompound(buffer,
                    [](const uint8_t, const std::string&, const PacketBuffer&) { return false; });
                break;
        }
        return packet;
    }
};
