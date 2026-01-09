#pragma once

#include <glm/glm.hpp>

struct Plane {
    mutable glm::vec3 normal;
    mutable float distance; // d in plane equation
};

using FrustumInfo = std::array<Plane, 6>;

inline void extractFrustumPlanes(FrustumInfo& planes, const glm::mat4& m) {

    // Left
    planes[0].normal.x = m[0][3] + m[0][0];
    planes[0].normal.y = m[1][3] + m[1][0];
    planes[0].normal.z = m[2][3] + m[2][0];
    planes[0].distance = m[3][3] + m[3][0];

    // Right
    planes[1].normal.x = m[0][3] - m[0][0];
    planes[1].normal.y = m[1][3] - m[1][0];
    planes[1].normal.z = m[2][3] - m[2][0];
    planes[1].distance = m[3][3] - m[3][0];

    // Bottom
    planes[2].normal.x = m[0][3] + m[0][1];
    planes[2].normal.y = m[1][3] + m[1][1];
    planes[2].normal.z = m[2][3] + m[2][1];
    planes[2].distance = m[3][3] + m[3][1];

    // Top
    planes[3].normal.x = m[0][3] - m[0][1];
    planes[3].normal.y = m[1][3] - m[1][1];
    planes[3].normal.z = m[2][3] - m[2][1];
    planes[3].distance = m[3][3] - m[3][1];

    // Near
    planes[4].normal.x = m[0][3] + m[0][2];
    planes[4].normal.y = m[1][3] + m[1][2];
    planes[4].normal.z = m[2][3] + m[2][2];
    planes[4].distance = m[3][3] + m[3][2];

    // Far
    planes[5].normal.x = m[0][3] - m[0][2];
    planes[5].normal.y = m[1][3] - m[1][2];
    planes[5].normal.z = m[2][3] - m[2][2];
    planes[5].distance = m[3][3] - m[3][2];

    for (auto&[normal, distance] : planes) {
        const float len = glm::length(normal);
        normal /= len;
        distance /= len;
    }
}

inline bool isBoxInFrustum(const FrustumInfo& frustum, const glm::vec3& relPos) {
    for (const auto& [n, d] : frustum) {
        const float px = relPos.x + (n.x >= 0 ? CHUNK_SIZE : 0.0f);
        const float py = relPos.y + (n.y >= 0 ? WORLD_HEIGHT : 0.0f);
        const float pz = relPos.z + (n.z >= 0 ? CHUNK_SIZE : 0.0f);

        if (n.x * px + n.y * py + n.z * pz + d < 0.0f)
            return false;
    }
    return true;
}