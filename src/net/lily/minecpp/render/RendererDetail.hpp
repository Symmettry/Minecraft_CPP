#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

#include "BlockAtlas.hpp"
#include "net/lily/minecpp/world/World.hpp"

namespace RendererDetail {
inline float interpolateRotation(const float previous, const float current, const float partialTicks) {
    float difference = current - previous;
    while (difference < -180.0f) difference += 360.0f;
    while (difference >= 180.0f) difference -= 360.0f;
    return previous + partialTicks * difference;
}

inline double intersectCameraRay(const glm::vec3& start, const glm::vec3& delta, const AABB& bounds) {
    double minimum = 0.0;
    double maximum = 1.0;
    const auto clipAxis = [&](const double origin, const double direction,
                              const double lower, const double upper) {
        if (std::abs(direction) < 1.0E-8) return origin >= lower && origin <= upper;
        double first = (lower - origin) / direction;
        double second = (upper - origin) / direction;
        if (first > second) std::swap(first, second);
        minimum = std::max(minimum, first);
        maximum = std::min(maximum, second);
        return maximum >= minimum;
    };
    if (!clipAxis(start.x, delta.x, bounds.minX, bounds.maxX)) return 1.0;
    if (!clipAxis(start.y, delta.y, bounds.minY, bounds.maxY)) return 1.0;
    if (!clipAxis(start.z, delta.z, bounds.minZ, bounds.maxZ)) return 1.0;
    return std::clamp(minimum, 0.0, 1.0);
}

inline float clipThirdPersonDistance(const World* world, const glm::vec3& eye,
                                     const glm::vec3& offset, const float distance) {
    const glm::vec3 end = eye + offset * distance;
    constexpr double cameraRadius = 0.1;
    const AABB searchBox(std::min(eye.x, end.x) - cameraRadius,
        std::min(eye.y, end.y) - cameraRadius, std::min(eye.z, end.z) - cameraRadius,
        std::max(eye.x, end.x) + cameraRadius, std::max(eye.y, end.y) + cameraRadius,
        std::max(eye.z, end.z) + cameraRadius);
    double fraction = 1.0;
    const glm::vec3 delta = offset * distance;
    for (const LocalBlock& collision : world->getCollidingBlocks(searchBox)) {
        const AABB expanded(collision.bounds.minX - cameraRadius,
            collision.bounds.minY - cameraRadius, collision.bounds.minZ - cameraRadius,
            collision.bounds.maxX + cameraRadius, collision.bounds.maxY + cameraRadius,
            collision.bounds.maxZ + cameraRadius);
        fraction = std::min(fraction, intersectCameraRay(eye, delta, expanded));
    }
    return static_cast<float>(fraction) * distance;
}

inline float biomeTemperature(uint8_t biome) {
    if (biome >= 128) biome -= 128;
    switch (biome) {
        case 2: case 17: case 37: case 38: case 39: return 2.0f;
        case 3: case 20: case 25: case 34: return 0.2f;
        case 4: case 18: case 27: case 29: return 0.7f;
        case 5: case 19: return 0.25f;
        case 6: return 0.8f;
        case 10: case 11: case 12: case 13: return 0.0f;
        case 14: case 15: return 0.9f;
        case 21: case 22: case 23: return 0.95f;
        case 26: return 0.05f;
        case 28: return 0.6f;
        case 30: case 31: return -0.5f;
        case 32: case 33: return 0.3f;
        case 35: case 36: return 1.2f;
        case 1: case 16: return 0.8f;
        default: return 0.5f;
    }
}

inline float biomeRainfall(uint8_t biome) {
    if (biome >= 128) biome -= 128;
    switch (biome) {
        case 2: case 17: case 35: case 36: case 37: case 38: case 39: return 0.0f;
        case 3: case 20: case 25: case 34: return 0.3f;
        case 4: case 18: case 27: case 28: case 29: return 0.8f;
        case 5: case 19: return 0.8f;
        case 6: case 21: case 22: case 23: return 0.9f;
        case 10: case 11: case 12: case 13: return 0.5f;
        case 14: case 15: return 1.0f;
        case 26: return 0.3f;
        case 30: case 31: return 0.4f;
        case 32: case 33: return 0.8f;
        case 1: case 16: return 0.4f;
        default: return 0.5f;
    }
}

inline glm::vec3 sampleClimateColor(const float rawTemperature, const float rawRainfall,
                                    const bool foliage) {
    static const TextureTile grass = BlockAtlas::loadPNG("assets/minecraft/textures/colormap/grass.png");
    static const TextureTile leaves = BlockAtlas::loadPNG("assets/minecraft/textures/colormap/foliage.png");
    const auto& [width, height, data] = foliage ? leaves : grass;
    const float temperature = std::clamp(rawTemperature, 0.0f, 1.0f);
    const float rainfall = std::clamp(rawRainfall, 0.0f, 1.0f) * temperature;
    const int x = std::clamp(static_cast<int>((1.0f - temperature) * 255.0f), 0, width - 1);
    const int y = std::clamp(static_cast<int>((1.0f - rainfall) * 255.0f), 0, height - 1);
    const size_t index = static_cast<size_t>((y * width + x) * 4);
    return glm::vec3(data[index], data[index + 1], data[index + 2]) / 255.0f;
}

inline glm::vec3 sampleBiomeColor(const uint8_t biome, const bool foliage) {
    return sampleClimateColor(biomeTemperature(biome), biomeRainfall(biome), foliage);
}

inline glm::vec3 hsvToRgb(const float hue, const float saturation, const float value) {
    const float scaled = hue * 6.0f;
    const int sector = static_cast<int>(scaled) % 6;
    const float fraction = scaled - std::floor(scaled);
    const float p = value * (1.0f - saturation);
    const float q = value * (1.0f - fraction * saturation);
    const float t = value * (1.0f - (1.0f - fraction) * saturation);
    switch (sector) {
        case 0: return {value, t, p};
        case 1: return {q, value, p};
        case 2: return {p, value, t};
        case 3: return {p, q, value};
        case 4: return {t, p, value};
        case 5: return {value, p, q};
        default: return {};
    }
}
}
