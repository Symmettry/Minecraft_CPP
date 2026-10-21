#include "SkinProfile.hpp"

#include <array>
#include <cstdint>

#include "json/JsonValue.hpp"

namespace {
std::string decodeBase64(const std::string& encoded) {
    std::array<int8_t, 256> values{};
    values.fill(-1);
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int index = 0; index < 64; ++index) values[static_cast<uint8_t>(alphabet[index])] = static_cast<int8_t>(index);

    std::string decoded;
    decoded.reserve(encoded.size() * 3 / 4);
    uint32_t accumulator = 0;
    int bits = 0;
    for (const unsigned char character : encoded) {
        if (character == '=') break;
        const int value = values[character];
        if (value < 0) continue;
        accumulator = accumulator << 6U | static_cast<uint32_t>(value);
        bits += 6;
        if (bits < 8) continue;
        bits -= 8;
        decoded.push_back(static_cast<char>((accumulator >> bits) & 0xffU));
    }
    return decoded;
}

const JsonValue* member(const JsonValue& value, const char* name, const JsonValue::Type type) {
    if (value.type != JsonValue::Type::Object) return nullptr;
    const auto found = value.object.find(name);
    return found != value.object.end() && found->second.type == type ? &found->second : nullptr;
}
}

SkinProfile SkinProfile::fromTextureProperty(const std::string& encoded) {
    try {
        const JsonValue root = JsonValue::parse(decodeBase64(encoded));
        const JsonValue* textures = member(root, "textures", JsonValue::Type::Object);
        const JsonValue* skin = textures != nullptr ? member(*textures, "SKIN", JsonValue::Type::Object) : nullptr;
        const JsonValue* url = skin != nullptr ? member(*skin, "url", JsonValue::Type::String) : nullptr;
        if (url == nullptr) return {};
        SkinProfile result{url->text, false};
        if (const JsonValue* metadata = member(*skin, "metadata", JsonValue::Type::Object)) {
            if (const JsonValue* model = member(*metadata, "model", JsonValue::Type::String)) result.slim = model->text == "slim";
        }
        return result;
    } catch (...) {
        return {};
    }
}
