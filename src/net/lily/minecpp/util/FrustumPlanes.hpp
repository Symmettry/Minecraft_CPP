#pragma once

#include <glm/glm.hpp>

struct Plane {
    mutable glm::vec3 normal;
    mutable float distance; // d in plane equation
};

inline std::array<Plane, 6> extractFrustumPlanes(const glm::mat4& m) {
    std::array<Plane, 6> planes;

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

    return planes;
}

inline bool isBoxInFrustum(const std::array<Plane, 6>& planes, const AABB& box) {
    for (const auto&[normal, distance] : planes) {
        glm::vec3 positiveVertex = {box.minX, box.minY, box.minZ};

        if (normal.x >= 0) positiveVertex.x = box.maxX;
        if (normal.y >= 0) positiveVertex.y = box.maxY;
        if (normal.z >= 0) positiveVertex.z = box.maxZ;

        if (glm::dot(normal, positiveVertex) + distance < 0.0f)
            return false;
    }
    return true;
}