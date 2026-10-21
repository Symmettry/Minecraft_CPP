#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace Utf8 {
inline uint32_t next(const std::string_view text, size_t& offset) {
    if (offset >= text.size()) return 0;
    const uint8_t first = static_cast<uint8_t>(text[offset++]);
    if (first < 0x80) return first;
    int remaining = 0;
    uint32_t value = 0;
    if ((first & 0xe0U) == 0xc0U) { remaining = 1; value = first & 0x1fU; }
    else if ((first & 0xf0U) == 0xe0U) { remaining = 2; value = first & 0x0fU; }
    else if ((first & 0xf8U) == 0xf0U) { remaining = 3; value = first & 0x07U; }
    else return 0xfffd;
    if (offset + remaining > text.size()) { offset = text.size(); return 0xfffd; }
    for (int index = 0; index < remaining; ++index) {
        const uint8_t continuation = static_cast<uint8_t>(text[offset]);
        if ((continuation & 0xc0U) != 0x80U) return 0xfffd;
        ++offset;
        value = value << 6U | continuation & 0x3fU;
    }
    return value;
}

inline void append(std::string& output, const uint32_t codepoint) {
    if (codepoint <= 0x7f) output.push_back(static_cast<char>(codepoint));
    else if (codepoint <= 0x7ff) {
        output.push_back(static_cast<char>(0xc0U | codepoint >> 6U));
        output.push_back(static_cast<char>(0x80U | codepoint & 0x3fU));
    } else if (codepoint <= 0xffff) {
        output.push_back(static_cast<char>(0xe0U | codepoint >> 12U));
        output.push_back(static_cast<char>(0x80U | codepoint >> 6U & 0x3fU));
        output.push_back(static_cast<char>(0x80U | codepoint & 0x3fU));
    } else if (codepoint <= 0x10ffff) {
        output.push_back(static_cast<char>(0xf0U | codepoint >> 18U));
        output.push_back(static_cast<char>(0x80U | codepoint >> 12U & 0x3fU));
        output.push_back(static_cast<char>(0x80U | codepoint >> 6U & 0x3fU));
        output.push_back(static_cast<char>(0x80U | codepoint & 0x3fU));
    }
}

inline void popBack(std::string& text) {
    if (text.empty()) return;
    size_t offset = text.size() - 1;
    while (offset > 0 && (static_cast<uint8_t>(text[offset]) & 0xc0U) == 0x80U) --offset;
    text.erase(offset);
}

inline size_t length(const std::string_view text) {
    size_t count = 0;
    for (size_t offset = 0; offset < text.size(); ++count) next(text, offset);
    return count;
}

inline void truncate(std::string& text, const size_t maximumCodepoints) {
    size_t offset = 0;
    for (size_t count = 0; count < maximumCodepoints && offset < text.size(); ++count) next(text, offset);
    if (offset < text.size()) text.erase(offset);
}
}
