#pragma once
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

#include <glm/glm.hpp>
#include "net/lily/minecpp/util/Math.hpp"
#include "net/lily/minecpp/world/chunk/Block.hpp"

struct PacketBuffer {

    PacketBuffer() : offset(0) {}
    ~PacketBuffer() = default;

    mutable std::vector<uint8_t> buffer;
    mutable size_t offset;

    size_t size() const {
        return buffer.size();
    }

    void confirm() const {
        if (offset != buffer.size()) throw std::runtime_error("[PacketBuffer] Expected " + std::to_string(offset) + " bytes, found " + std::to_string(buffer.size()) + " bytes");
    }

    void require(const std::string& label, const size_t bytes) const {
        if (offset + bytes > buffer.size()) throw std::runtime_error("[PacketBuffer] Buffer size too small when reading " + label);
    }
    void testOverflow(const std::string& label) const {
        if (offset >= buffer.size()) throw std::runtime_error("[PacketBuffer] offset exceeds buffer size when reading " + label);
    }

    void writeVarInt(uint32_t value) const {
        do {
            uint8_t temp = value & 0x7F;
            value >>= 7;
            if (value != 0) temp |= 0x80;
            buffer.push_back(temp);
        } while (value != 0);
    }

    void writeString(const std::string& str) const {
        writeVarInt(str.size());
        buffer.insert(buffer.end(), str.begin(), str.end());
    }

    void writeUShort(const uint16_t s) const {
        writeByte(s >> 8 & 0xFF);
        writeByte(s & 0xFF);
    }

    void writeByteArray(const std::vector<uint8_t>& arr) const {
        writeVarInt(arr.size());
        buffer.insert(buffer.end(), arr.begin(), arr.end());
    }

    void writeDouble(const double value) const {
        uint64_t bits;
        std::memcpy(&bits, &value, sizeof(double));
        for (int i = 7; i >= 0; --i) {
            buffer.push_back(static_cast<uint8_t>((bits >> (i * 8)) & 0xFF));
        }
    }

    void writeFloat(const float value) const {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(float));
        for (int i = 3; i >= 0; --i) {
            buffer.push_back(static_cast<uint8_t>((bits >> (i * 8)) & 0xFF));
        }
    }

    void writeByte(const int value) const {
        buffer.push_back(value);
    }
    void writeBool(const bool value) const {
        writeByte(value ? 1 : 0);
    }

    uint32_t readVarInt() const {
        uint32_t value = 0;
        int shift = 0;
        int count = 0;

        while (true) {
            testOverflow("VarInt");

            const uint8_t byte = buffer[offset++];
            value |= (byte & 0x7F) << shift;

            if (!(byte & 0x80)) break;

            shift += 7;
            count++;
            if (count > 5) throw std::runtime_error("[PacketBuffer] VarInt too big");
        }

        return value;
    }

    glm::vec<3, int> readPosition() const {
        const uint64_t val = readULong();

        int x = val >> 38, y = (val >> 26) & 0xFFF, z = val & 0x3FFFFFF;

        if (x >= (1 << 25)) x -= (1 << 26);
        if (y >= (1 << 11)) y -= (1 << 12);
        if (z >= (1 << 25)) z -= (1 << 26);

        return { x, y, z };
    }

    std::string readString(const size_t maxLength = SIZE_MAX) const {
        const uint32_t length = readVarInt();
        if (length > maxLength)
            throw std::runtime_error("[PacketBuffer] readString: string length exceeds maxLength - " + std::to_string(length) + ">" + std::to_string(maxLength));
        require("String", length);
        std::string result(buffer.begin() + offset, buffer.begin() + offset + length);
        offset += length;
        return result;
    }

    std::vector<uint8_t> readByteArray() const {
        const uint32_t length = readVarInt();
        require("ByteArray", length);
        std::vector result(buffer.begin() + offset, buffer.begin() + offset + length);
        offset += length;
        return result;
    }

    double readDouble() const {
        require("Double", 8);
        uint64_t temp = 0;
        for (int i = 0; i < 8; ++i) {
            temp = (temp << 8) | buffer[offset + i];
        }
        offset += 8;
        double value;
        std::memcpy(&value, &temp, sizeof(double));
        return value;
    }

    float readFloat() const {
        require("Float", 4);
        uint32_t temp = 0;
        for (int i = 0; i < 4; ++i) {
            temp = (temp << 8) | buffer[offset + i];
        }
        offset += 4;
        float value;
        std::memcpy(&value, &temp, sizeof(float));
        return value;
    }

    bool readBool() const {
        return readByte() == 1;
    }

    uint64_t readULong() const {
        require("Long", 8);
        uint64_t temp = 0;
        for (size_t i = 0; i < 8; ++i) {
            temp = (temp << 8) | buffer[offset + i];
        }
        offset += 8;
        return temp;
    }

    int32_t readInt() const {
        require("Integer", 4);
        const int32_t value = (buffer[offset] << 24) | (buffer[offset + 1] << 16) | (buffer[offset + 2] << 8) | buffer[offset + 3];
        offset += 4;
        return value;
    }
    int32_t readShort() const {
        require("Short", 2);
        const int32_t value = (buffer[offset] << 8) | buffer[offset + 1];
        offset += 2;
        return value;
    }
    Block readBlock() const {
        require("Block", 2);
        const uint16_t value = buffer[offset] | (buffer[offset + 1] << 8);
        offset += 2;
        return value;
    }

    uint8_t readByte() const {
        require("Byte", 1);
        return buffer[offset++];
    }

    UUID readUUID() const {
        require("UUID", 16);
        std::array<uint8_t, 16> uuid{};
        std::memcpy(uuid.data(), buffer.data() + offset, 16);
        offset += 16;
        return {uuid};
    }

};
