#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

inline std::vector<unsigned char> generateMipmapLevel(const std::vector<unsigned char>& source,
                                                       const int width, const int height,
                                                       const bool hasTransparency) {
    const int nextWidth = width / 2;
    const int nextHeight = height / 2;
    std::vector<unsigned char> result(nextWidth * nextHeight * 4);
    for (int y = 0; y < nextHeight; ++y) {
        for (int x = 0; x < nextWidth; ++x) {
            float sums[4]{};
            for (int offsetY = 0; offsetY < 2; ++offsetY) {
                for (int offsetX = 0; offsetX < 2; ++offsetX) {
                    const int sourceIndex = ((y * 2 + offsetY) * width + x * 2 + offsetX) * 4;
                    if (hasTransparency && source[sourceIndex + 3] == 0) continue;
                    for (int channel = 0; channel < 4; ++channel) {
                        sums[channel] += std::pow(source[sourceIndex + channel] / 255.0f, 2.2f);
                    }
                }
            }
            const int destination = (y * nextWidth + x) * 4;
            for (int channel = 0; channel < 4; ++channel) {
                result[destination + channel] = static_cast<unsigned char>(
                    std::pow(sums[channel] * 0.25f, 1.0f / 2.2f) * 255.0f);
            }
            if (hasTransparency && result[destination + 3] < 96) result[destination + 3] = 0;
        }
    }
    return result;
}
