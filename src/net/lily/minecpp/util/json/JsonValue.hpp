#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct JsonValue {
    enum class Type { Null, String, Number, Boolean, Array, Object } type = Type::Null;

    std::string text;
    bool boolean = false;
    std::vector<JsonValue> array;
    std::unordered_map<std::string, JsonValue> object;

    static JsonValue parse(const std::string& source);
};
