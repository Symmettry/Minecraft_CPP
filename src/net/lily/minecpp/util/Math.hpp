//
// created by lily on 12/27/25
//

#ifndef MINECRAFTCLIENT_MATH_HPP
#define MINECRAFTCLIENT_MATH_HPP
#include <glm/vec4.hpp>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <type_traits>
#include <string>
#include <array>

#define FLOAT static_cast<float>

class Math {
public:
    static float sin(const float value) {
        return sineTable()[static_cast<uint16_t>(static_cast<int>(value * 10430.378F))];
    }

    static float cos(const float value) {
        return sineTable()[static_cast<uint16_t>(
            static_cast<int>(value * 10430.378F + 16384.0F))];
    }

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

private:
    static const std::array<float, 65536>& sineTable() {
        static const std::array<float, 65536> table = [] {
            std::array<float, 65536> values{};
            for (size_t index = 0; index < values.size(); ++index) {
                values[index] = static_cast<float>(std::sin(
                    static_cast<double>(index) * M_PI * 2.0 / 65536.0));
            }
            return values;
        }();
        return table;
    }
};

struct bUUID {
    std::array<uint8_t, 16> bytes;
};

#endif //mINECRAFTCLIENT_MATH_HPP
