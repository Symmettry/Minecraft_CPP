#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"

struct S38PacketPlayerListItem {
    struct Entry {
        struct Property {
            std::string name;
            std::string value;
            std::string signature;
        };

        std::array<uint8_t, 16> uuid{};
        std::string name;
        std::string displayName;
        std::vector<Property> properties;
        int32_t gameMode = 0;
        int32_t ping = 0;
    };

    uint32_t action{};
    std::vector<Entry> entries;

    static S38PacketPlayerListItem deserialize(const PacketBuffer& buffer) {
        S38PacketPlayerListItem packet;
        packet.action = buffer.readVarInt();
        const uint32_t count = buffer.readVarInt();
        packet.entries.reserve(count);
        for (uint32_t index = 0; index < count; ++index) {
            Entry entry;
            buffer.readBytes(entry.uuid.data(), entry.uuid.size());
            switch (packet.action) {
                case 0: {
                    entry.name = buffer.readString(16);
                    const uint32_t properties = buffer.readVarInt();
                    entry.properties.reserve(properties);
                    for (uint32_t property = 0; property < properties; ++property) {
                        Entry::Property value{buffer.readString(32767), buffer.readString(32767), {}};
                        if (buffer.readBool()) value.signature = buffer.readString(32767);
                        entry.properties.push_back(std::move(value));
                    }
                    entry.gameMode = static_cast<int32_t>(buffer.readVarInt());
                    entry.ping = static_cast<int32_t>(buffer.readVarInt());
                    if (buffer.readBool()) entry.displayName = buffer.readString(32767);
                    break;
                }
                case 1: entry.gameMode = static_cast<int32_t>(buffer.readVarInt()); break;
                case 2: entry.ping = static_cast<int32_t>(buffer.readVarInt()); break;
                case 3: if (buffer.readBool()) entry.displayName = buffer.readString(32767); break;
                case 4: break;
                default: throw std::runtime_error("Unknown S38 player-list action");
            }
            packet.entries.push_back(std::move(entry));
        }
        return packet;
    }
};
