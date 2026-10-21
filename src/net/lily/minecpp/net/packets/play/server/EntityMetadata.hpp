#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <array>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "net/lily/minecpp/net/packets/PacketBuffer.hpp"

struct NetworkItemStack {
    int16_t itemId = -1;
    int8_t count = 0;
    int16_t damage = 0;
    std::vector<uint8_t> nbt;
    std::string customName;
    std::vector<std::string> lore;
    bool enchanted = false;
    explicit operator bool() const { return itemId >= 0; }
};

namespace EntityMetadataDetail {
inline int16_t readShort(const PacketBuffer& buffer) {
    return static_cast<int16_t>(buffer.readUShort());
}

inline void skipNbtPayload(const PacketBuffer& buffer, uint8_t type);

inline void skipNbtString(const PacketBuffer& buffer) {
    const uint16_t length = static_cast<uint16_t>(buffer.readUShort());
    buffer.require("NBT string", length);
    buffer.offset += length;
}

inline void skipNbtPayload(const PacketBuffer& buffer, const uint8_t type) {
    switch (type) {
        case 0: return;
        case 1: buffer.require("NBT byte", 1); buffer.offset += 1; return;
        case 2: buffer.require("NBT short", 2); buffer.offset += 2; return;
        case 3: case 5: buffer.require("NBT int/float", 4); buffer.offset += 4; return;
        case 4: case 6: buffer.require("NBT long/double", 8); buffer.offset += 8; return;
        case 7: {
            const int32_t length = buffer.readInt();
            if (length < 0) throw std::runtime_error("Negative NBT byte-array length");
            buffer.require("NBT byte array", static_cast<size_t>(length));
            buffer.offset += static_cast<size_t>(length);
            return;
        }
        case 8: skipNbtString(buffer); return;
        case 9: {
            const uint8_t elementType = buffer.readByte();
            const int32_t length = buffer.readInt();
            if (length < 0) throw std::runtime_error("Negative NBT list length");
            for (int32_t index = 0; index < length; ++index) skipNbtPayload(buffer, elementType);
            return;
        }
        case 10:
            while (true) {
                const uint8_t childType = buffer.readByte();
                if (childType == 0) return;
                skipNbtString(buffer);
                skipNbtPayload(buffer, childType);
            }
        case 11: case 12: {
            const int32_t length = buffer.readInt();
            if (length < 0) throw std::runtime_error("Negative NBT array length");
            const size_t width = type == 11 ? 4U : 8U;
            buffer.require("NBT numeric array", static_cast<size_t>(length) * width);
            buffer.offset += static_cast<size_t>(length) * width;
            return;
        }
        default: throw std::runtime_error("Unknown NBT type " + std::to_string(type));
    }
}

inline std::string readNbtString(const PacketBuffer& buffer) {
    const uint16_t length = static_cast<uint16_t>(buffer.readUShort());
    buffer.require("NBT string", length);
    const std::string value(buffer.buffer.begin() + static_cast<std::ptrdiff_t>(buffer.offset),
        buffer.buffer.begin() + static_cast<std::ptrdiff_t>(buffer.offset + length));
    buffer.offset += length;
    return value;
}

inline void readDisplayTag(const PacketBuffer& buffer, NetworkItemStack& stack) {
    while (true) {
        const uint8_t type = buffer.readByte();
        if (type == 0) return;
        const std::string name = readNbtString(buffer);
        if (name == "Name" && type == 8) {
            stack.customName = readNbtString(buffer);
            continue;
        }
        if (name == "Lore" && type == 9) {
            const uint8_t elementType = buffer.readByte();
            const int32_t count = buffer.readInt();
            if (count < 0) throw std::runtime_error("Negative NBT lore length");
            stack.lore.reserve(static_cast<size_t>(count));
            for (int32_t line = 0; line < count; ++line) {
                if (elementType == 8) stack.lore.push_back(readNbtString(buffer));
                else skipNbtPayload(buffer, elementType);
            }
            continue;
        }
        skipNbtPayload(buffer, type);
    }
}

inline void readItemDisplay(const std::vector<uint8_t>& bytes, NetworkItemStack& stack) {
    if (bytes.empty()) return;
    PacketBuffer buffer;
    buffer.buffer = bytes;
    const uint8_t rootType = buffer.readByte();
    if (rootType == 0) return;
    skipNbtString(buffer);
    if (rootType != 10) return;
    while (true) {
        const uint8_t type = buffer.readByte();
        if (type == 0) return;
        const std::string name = readNbtString(buffer);
        if (name == "display" && type == 10) readDisplayTag(buffer, stack);
        else skipNbtPayload(buffer, type);
    }
}

inline NetworkItemStack readItemStack(const PacketBuffer& buffer) {
    NetworkItemStack stack;
    stack.itemId = readShort(buffer);
    if (stack.itemId < 0) return stack;
    stack.count = static_cast<int8_t>(buffer.readByte());
    stack.damage = readShort(buffer);
    const size_t nbtStart = buffer.offset;
    const uint8_t nbtType = buffer.readByte();
    if (nbtType != 0) {
        skipNbtString(buffer);
        skipNbtPayload(buffer, nbtType);
    }
    stack.nbt.assign(buffer.buffer.begin() + static_cast<std::ptrdiff_t>(nbtStart),
        buffer.buffer.begin() + static_cast<std::ptrdiff_t>(buffer.offset));
    if (nbtType != 0) {
        readItemDisplay(stack.nbt, stack);
        static constexpr char enchantments[] = {'e', 'n', 'c', 'h'};
        static constexpr char stored[] = {'S', 't', 'o', 'r', 'e', 'd', 'E', 'n', 'c', 'h', 'a', 'n', 't', 'm', 'e', 'n', 't', 's'};
        stack.enchanted = std::search(stack.nbt.begin(), stack.nbt.end(), std::begin(enchantments), std::end(enchantments))
            != stack.nbt.end() || std::search(stack.nbt.begin(), stack.nbt.end(), std::begin(stored), std::end(stored))
            != stack.nbt.end();
    }
    return stack;
}

inline void writeItemStack(const PacketBuffer& buffer, const NetworkItemStack& stack) {
    buffer.writeUShort(static_cast<uint16_t>(stack ? stack.itemId : -1));
    if (!stack) return;
    buffer.writeByte(stack.count);
    buffer.writeUShort(static_cast<uint16_t>(stack.damage));
    if (stack.nbt.empty()) buffer.writeByte(0);
    else buffer.buffer.insert(buffer.buffer.end(), stack.nbt.begin(), stack.nbt.end());
}
}

struct EntityMetadata {
    uint8_t flags = 0;
    std::optional<NetworkItemStack> item;
    std::array<std::optional<int8_t>, 32> bytes{};
    std::array<std::optional<int16_t>, 32> shorts{};
    std::array<std::optional<int32_t>, 32> ints{};
    std::array<std::optional<float>, 32> floats{};
    std::array<std::optional<std::string>, 32> strings{};
    std::array<std::optional<glm::vec3>, 32> rotations{};

    static EntityMetadata deserialize(const PacketBuffer& buffer) {
        EntityMetadata metadata;
        while (true) {
            const uint8_t header = buffer.readByte();
            if (header == 0x7F) break;
            const uint8_t type = header >> 5;
            const uint8_t index = header & 31;
            switch (type) {
                case 0: {
                    const uint8_t value = buffer.readByte();
                    metadata.bytes[index] = static_cast<int8_t>(value);
                    if (index == 0) metadata.flags = value;
                    break;
                }
                case 1: metadata.shorts[index] = EntityMetadataDetail::readShort(buffer); break;
                case 2: metadata.ints[index] = buffer.readInt(); break;
                case 3: metadata.floats[index] = buffer.readFloat(); break;
                case 4: metadata.strings[index] = buffer.readString(32767); break;
                case 5: {
                    NetworkItemStack stack = EntityMetadataDetail::readItemStack(buffer);
                    if (index == 10) metadata.item = stack;
                    break;
                }
                case 6: buffer.readInt(); buffer.readInt(); buffer.readInt(); break;
                case 7:
                    metadata.rotations[index] = {
                        buffer.readFloat(), buffer.readFloat(), buffer.readFloat()};
                    break;
                default: throw std::runtime_error("Unknown entity metadata type");
            }
        }
        return metadata;
    }
};
