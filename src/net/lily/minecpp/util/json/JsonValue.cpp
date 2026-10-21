#include "JsonValue.hpp"

#include <cctype>

namespace {
class JsonParser final {
public:
    explicit JsonParser(const std::string& source) : source(source) {}

    JsonValue parse() {
        skipWhitespace();
        return parseValue();
    }

private:
    const std::string& source;
    size_t offset = 0;

    void skipWhitespace() {
        while (offset < source.size() && std::isspace(static_cast<unsigned char>(source[offset]))) ++offset;
    }

    bool take(const char expected) {
        skipWhitespace();
        if (offset >= source.size() || source[offset] != expected) return false;
        ++offset;
        return true;
    }

    std::string parseString() {
        if (!take('"')) return {};
        std::string result;
        while (offset < source.size()) {
            const char character = source[offset++];
            if (character == '"') break;
            if (character != '\\' || offset >= source.size()) {
                result += character;
                continue;
            }
            switch (const char escaped = source[offset++]) {
                case '"': case '\\': case '/': result += escaped; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'u': {
                    unsigned int codepoint = 0;
                    for (int index = 0; index < 4 && offset < source.size(); ++index) {
                        const char digit = source[offset++];
                        codepoint = codepoint * 16 + (digit >= '0' && digit <= '9' ? digit - '0'
                            : digit >= 'a' && digit <= 'f' ? digit - 'a' + 10 : digit - 'A' + 10);
                    }
                    result += codepoint < 128 ? static_cast<char>(codepoint) : '?';
                    break;
                }
                default: break;
            }
        }
        return result;
    }

    JsonValue parseValue() {
        skipWhitespace();
        if (offset >= source.size()) return {};
        if (source[offset] == '"') {
            JsonValue result;
            result.type = JsonValue::Type::String;
            result.text = parseString();
            return result;
        }
        if (source[offset] == '[') {
            JsonValue result;
            result.type = JsonValue::Type::Array;
            ++offset;
            skipWhitespace();
            while (offset < source.size() && source[offset] != ']') {
                result.array.push_back(parseValue());
                skipWhitespace();
                if (!take(',')) break;
            }
            take(']');
            return result;
        }
        if (source[offset] == '{') {
            JsonValue result;
            result.type = JsonValue::Type::Object;
            ++offset;
            skipWhitespace();
            while (offset < source.size() && source[offset] != '}') {
                const std::string key = parseString();
                take(':');
                result.object[key] = parseValue();
                skipWhitespace();
                if (!take(',')) break;
            }
            take('}');
            return result;
        }

        const size_t begin = offset;
        while (offset < source.size() && !std::isspace(static_cast<unsigned char>(source[offset]))
               && source[offset] != ',' && source[offset] != ']' && source[offset] != '}') ++offset;
        JsonValue result;
        result.text = source.substr(begin, offset - begin);
        if (result.text == "true" || result.text == "false") {
            result.type = JsonValue::Type::Boolean;
            result.boolean = result.text == "true";
        } else if (result.text != "null") {
            result.type = JsonValue::Type::Number;
        }
        return result;
    }
};
}

JsonValue JsonValue::parse(const std::string& source) {
    return JsonParser(source).parse();
}
