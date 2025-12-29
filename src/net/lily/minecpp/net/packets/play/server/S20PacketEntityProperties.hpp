#pragma once
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include "../../Packet.hpp"
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"
#include "net/lily/minecpp/util/Attributes.hpp"

class S20PacketEntityProperties : public ClientBoundPacket {
public:
    int entityId = 0;

    struct Snapshot {
        std::string attributeName;
        double baseValue = 0.0;
        std::vector<AttributeModifier> modifiers;
    };

    std::vector<Snapshot> snapshots;

    S20PacketEntityProperties() : ClientBoundPacket(0x20) {}

    static S20PacketEntityProperties deserialize(const std::vector<uint8_t>& data) {
        S20PacketEntityProperties packet;
        size_t offset = 0;

        packet.entityId = readVarInt(data, offset);
        const int snapshotCount = readInt(data, offset);

        for (int i = 0; i < snapshotCount; ++i) {
            Snapshot snap;
            snap.attributeName = readString(data, offset, 64);
            snap.baseValue = readDouble(data, offset);

            int modifierCount = readVarInt(data, offset);
            for (int j = 0; j < modifierCount; ++j) {
                AttributeModifier mod;
                mod.uuid = readUUID(data, offset);
                mod.amount = readDouble(data, offset);
                mod.operation = readByte(data, offset);
                snap.modifiers.push_back(mod);
            }

            packet.snapshots.push_back(snap);
        }

        return packet;
    }
};
