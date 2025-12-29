//
// Created by lily on 12/27/25.
//

#ifndef MINECRAFTCLIENT_MATH_HPP
#define MINECRAFTCLIENT_MATH_HPP
#include <glm/vec4.hpp>
#include <sstream>
#include <iomanip>
#include <type_traits>
#include <string>

#define FLOAT static_cast<float>

class Math {
public:
    static glm::vec4 mcColorToVec4(unsigned int color, const bool dropShadow) {
        if (dropShadow) {
            constexpr unsigned int RGB_MASK   = 0xFCFCFC;
            constexpr unsigned int ALPHA_MASK = 0xFF000000;
            color = (color & RGB_MASK) >> 2 | color & ALPHA_MASK;
        }

        const float r = FLOAT((color >> 16) & 0xFF) / 255.0f;
        const float g = FLOAT((color >> 8)  & 0xFF) / 255.0f;
        const float b = FLOAT(color & 0xFF)         / 255.0f;
        const float a = FLOAT((color >> 24) & 0xFF) / 255.0f;

        return { r, g, b, a };
    }

    template <typename T>
    requires std::is_integral_v<T>
    static std::string toHexString(const T value, const bool uppercase = false) {
        std::ostringstream stream;
        stream << std::hex;
        if (uppercase)
        {
            stream << std::uppercase;
        }
        stream << static_cast<std::make_unsigned_t<T>>(value);
        std::string str = stream.str();
        return str.length() == 1 ? "0" + str : str;
    }
};

struct UUID {
    std::array<uint8_t, 16> bytes;
};

#endif //MINECRAFTCLIENT_MATH_HPP