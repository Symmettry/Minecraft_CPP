#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"
// string limits are in characters in the protocol, up to four bytes each (a section sign is two), so the byte limit is 4x

struct S3EPacketTeams {
    std::string name;
    std::string displayName;
    std::string prefix;
    std::string suffix;
    std::string nameTagVisibility = "always";
    uint8_t action = 0;
    uint8_t friendlyFlags = 0;
    int8_t color = -1;
    std::vector<std::string> players;

    static S3EPacketTeams deserialize(const PacketBuffer& buffer) {
        S3EPacketTeams packet;
        packet.name = buffer.readString(64);
        packet.action = buffer.readByte();
        switch (packet.action) {
            case 0: case 2:
                packet.displayName = buffer.readString(128);
                packet.prefix = buffer.readString(64);
                packet.suffix = buffer.readString(64);
                packet.friendlyFlags = buffer.readByte();
                packet.nameTagVisibility = buffer.readString(128);
                packet.color = static_cast<int8_t>(buffer.readByte());
                break;
            default: break;
        }
        switch (packet.action) {
            case 0: case 3: case 4: {
                const uint32_t count = buffer.readVarInt();
                packet.players.reserve(count);
                for (uint32_t index = 0; index < count; ++index) packet.players.push_back(buffer.readString(160));
                break;
            }
            default: break;
        }
        return packet;
    }
};
