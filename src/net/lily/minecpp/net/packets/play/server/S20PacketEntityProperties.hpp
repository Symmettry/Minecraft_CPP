#pragma once
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include "../../Packet.hpp"
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"
#include "net/lily/minecpp/util/Attributes.hpp"

class S20PacketEntityProperties final : public ClientBoundPacket {
public:
    int entityId = 0;

    struct Snapshot {
        std::string attributeName;
        double baseValue = 0.0;
        std::vector<AttributeModifier> modifiers;
    };

    std::vector<Snapshot> snapshots;

    S20PacketEntityProperties() : ClientBoundPacket(0x20) {}

    static S20PacketEntityProperties deserialize(const PacketBuffer& buf) {
        S20PacketEntityProperties packet;
        packet.entityId = buf.readVarInt();
        const int snapshotCount = buf.readInt();

        for (int i = 0; i < snapshotCount; ++i) {
            Snapshot snap;
            snap.attributeName = buf.readString(64);
            snap.baseValue = buf.readDouble();

            const int modifierCount = buf.readVarInt();
            for (int j = 0; j < modifierCount; ++j) {
                AttributeModifier mod;
                mod.uuid = buf.readUUID();
                mod.amount = buf.readDouble();
                mod.operation = buf.readByte();
                snap.modifiers.push_back(mod);
            }

            packet.snapshots.push_back(snap);
        }

        return packet;
    }
};
