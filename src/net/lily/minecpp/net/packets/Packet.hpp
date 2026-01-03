#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <string>
#include <memory>
#include <iostream>

#include "net/lily/minecpp/util/Math.hpp"

class Packet {
public:
    uint32_t id = 0;

    explicit Packet(const uint32_t id) : id(id) {}
    virtual ~Packet() = default;

    static uint32_t readVarInt(const std::vector<uint8_t>& data, size_t& offset) {
        uint32_t value = 0;
        int shift = 0;
        int count = 0;

        while (true) {
            if (offset >= data.size()) throw std::runtime_error("readVarInt: offset exceeds buffer size");

            const uint8_t byte = data[offset++];
            value |= (byte & 0x7F) << shift;

            if (!(byte & 0x80)) break;

            shift += 7;
            count++;
            if (count > 5) throw std::runtime_error("VarInt too big");
        }

        return value;
    }

    static void writeVarInt(uint32_t value, std::vector<uint8_t>& buffer) {
        do {
            uint8_t temp = value & 0x7F;
            value >>= 7;
            if (value != 0) temp |= 0x80;
            buffer.push_back(temp);
        } while (value != 0);
    }

    static void writeString(const std::string& str, std::vector<uint8_t>& buffer) {
        writeVarInt(str.size(), buffer);
        buffer.insert(buffer.end(), str.begin(), str.end());
    }

    static std::string readString(const std::vector<uint8_t>& data, size_t& offset, size_t maxLength = SIZE_MAX) {
        const uint32_t length = readVarInt(data, offset);
        if (length > maxLength)
            throw std::runtime_error("readString: string length exceeds maxLength - " + std::to_string(length) + ">" + std::to_string(maxLength));
        if (offset + length > data.size())
            throw std::runtime_error("readString: offset + length out of bounds");
        std::string result(data.begin() + offset, data.begin() + offset + length);
        offset += length;
        return result;
    }

    static std::vector<uint8_t> readByteArray(const std::vector<uint8_t>& data, size_t& offset) {
        const uint32_t length = readVarInt(data, offset);
        if (offset + length > data.size()) throw std::runtime_error("Byte array exceeds buffer size");
        std::vector result(data.begin() + offset, data.begin() + offset + length);
        offset += length;
        return result;
    }

    static void writeByteArray(const std::vector<uint8_t>& arr, std::vector<uint8_t>& buffer) {
        writeVarInt(arr.size(), buffer);
        buffer.insert(buffer.end(), arr.begin(), arr.end());
    }

    static double readDouble(const std::vector<uint8_t>& data, size_t& offset) {
        if (offset + 8 > data.size()) throw std::runtime_error("Buffer too small for double");
        uint64_t temp = 0;
        for (int i = 0; i < 8; ++i) {
            temp = (temp << 8) | data[offset + i];
        }
        offset += 8;
        double value;
        std::memcpy(&value, &temp, sizeof(double));
        return value;
    }

    static float readFloat(const std::vector<uint8_t>& data, size_t& offset) {
        if (offset + 4 > data.size()) throw std::runtime_error("Buffer too small for float");
        uint32_t temp = 0;
        for (int i = 0; i < 4; ++i) {
            temp = (temp << 8) | data[offset + i];
        }
        offset += 4;
        float value;
        std::memcpy(&value, &temp, sizeof(float));
        return value;
    }

    static void writeDouble(const double value, std::vector<uint8_t>& buffer) {
        uint64_t bits;
        std::memcpy(&bits, &value, sizeof(double));
        for (int i = 7; i >= 0; --i) {
            buffer.push_back(static_cast<uint8_t>((bits >> (i * 8)) & 0xFF));
        }
    }

    static void writeFloat(const float value, std::vector<uint8_t>& buffer) {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(float));
        for (int i = 3; i >= 0; --i) {
            buffer.push_back(static_cast<uint8_t>((bits >> (i * 8)) & 0xFF));
        }
    }

    static void writeByte(const int value, std::vector<uint8_t>& buffer) {
        buffer.push_back(value);
    }
    static void writeBool(const bool value, std::vector<uint8_t>& buffer) {
        writeByte(value ? 1 : 0, buffer);
    }

    static bool readBool(const std::vector<uint8_t>& data, size_t& offset) {
        return readByte(data, offset) == 1;
    }

    static int32_t readInt(const std::vector<uint8_t>& data, size_t& offset) {
        if (offset + 4 > data.size()) throw std::runtime_error("Buffer too small for int");
        const int32_t value = (data[offset] << 24) | (data[offset + 1] << 16) | (data[offset + 2] << 8) | data[offset + 3];
        offset += 4;
        return value;
    }
    static int32_t readShort(const std::vector<uint8_t>& data, size_t& offset) {
        if (offset + 2 > data.size()) throw std::runtime_error("Buffer too small for short");
        const int32_t value = (data[offset] << 8) | data[offset + 1];
        offset += 2;
        return value;
    }
    static uint16_t readUnsignedShort(const std::vector<uint8_t>& data, size_t& offset) {
        if (offset + 2 > data.size()) throw std::runtime_error("Buffer too small for short");
        const uint16_t value = (data[offset] << 8) | data[offset + 1];
        offset += 2;
        return value;
    }

    static uint8_t readByte(const std::vector<uint8_t>& data, size_t& offset) {
        if (offset >= data.size()) throw std::runtime_error("Buffer too small for byte");
        return data[offset++];
    }

    static UUID readUUID(const std::vector<uint8_t>& data, size_t& offset) {
        if (offset + 16 > data.size()) throw std::runtime_error("Buffer too small for UUID");
        std::array<uint8_t, 16> uuid{};
        std::memcpy(uuid.data(), data.data() + offset, 16);
        offset += 16;
        return {uuid};
    }

};
