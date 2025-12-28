#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <memory>
#include <iostream>

class Packet {
public:
    uint32_t id = 0;

    explicit Packet(uint32_t id) : id(id) {}
    virtual ~Packet() = default;

    static uint32_t readVarInt(const std::vector<uint8_t>& data, size_t& offset) {
        uint32_t value = 0;
        int shift = 0;
        int count = 0;

        while (true) {
            if (offset >= data.size()) throw std::runtime_error("readVarInt: offset exceeds buffer size");

            uint8_t byte = data[offset++];
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

};