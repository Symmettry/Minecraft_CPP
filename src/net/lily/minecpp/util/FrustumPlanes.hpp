#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

struct Plane {
    mutable glm::vec3 normal;
    mutable float distance; // d in plane equation
};

inline std::array<Plane, 6> extractFrustumPlanes(const glm::mat4& projView) {
    std::array<Plane, 6> planes;

    const auto m = glm::value_ptr(projView);

    planes[0] = { glm::vec3(m[3] + m[0], m[7] + m[4], m[11] + m[8]), m[15] + m[12] }; // left
    planes[1] = { glm::vec3(m[3] - m[0], m[7] - m[4], m[11] - m[8]), m[15] - m[12] }; // right
    planes[2] = { glm::vec3(m[3] - m[1], m[7] - m[5], m[11] - m[9]), m[15] - m[13] }; // top
    planes[3] = { glm::vec3(m[3] + m[1], m[7] + m[5], m[11] + m[9]), m[15] + m[13] }; // bottom
    planes[4] = { glm::vec3(m[3] + m[2], m[7] + m[6], m[11] + m[10]), m[15] + m[14] }; // near
    planes[5] = { glm::vec3(m[3] - m[2], m[7] - m[6], m[11] - m[10]), m[15] - m[14] }; // far

    for (auto&[normal, distance] : planes) {
        const float len = glm::length(normal);
        normal /= len;
        distance /= len;
    }

    return planes;
}

inline bool isBoxInFrustum(const std::array<Plane, 6>& planes, const glm::vec3& min, const glm::vec3& max) {
    for (const auto&[normal, distance] : planes) {
        glm::vec3 positiveVertex = min;

        if (normal.x >= 0) positiveVertex.x = max.x;
        if (normal.y >= 0) positiveVertex.y = max.y;
        if (normal.z >= 0) positiveVertex.z = max.z;

        if (glm::dot(normal, positiveVertex) + distance < 0.0f)
            return false;
    }
    return true;
}