#pragma once

#include <glm/glm.hpp>
#include <array>
#include <numeric>
#include <algorithm>

#include "Camera.h"
#include "Splat.h"

class BB {
public:
    glm::mat4 transform;

    static BB from_aabb(glm::vec3 min, glm::vec3 max) {
        BB bb;
        bb.transform = glm::mat4(1.0f);
        bb.transform[3] = glm::vec4((min + max) / 2.0f, 1.0f);
        bb.transform[0][0] = max.x - min.x;
        bb.transform[1][1] = max.y - min.y;
        bb.transform[2][2] = max.z - min.z;
        return bb;
    }

    glm::vec3 center() const {
        return glm::vec3(transform[3]);
    }
    glm::vec3 size() const {
        float x = glm::length(glm::vec3(transform[0]));
        float y = glm::length(glm::vec3(transform[1]));
        float z = glm::length(glm::vec3(transform[2]));
        return glm::vec3(x, y, z);
    }
    glm::vec3 min() const {
        return center() - size() / 2.0f;
    }
    glm::vec3 max() const {
        return center() + size() / 2.0f;
    }

    std::array<glm::vec3, 8> corners() const {
        std::array<glm::vec3, 8> corners;
        glm::vec3 half_size = size() / 2.0f;
        float x = half_size.x;
        float y = half_size.y;
        float z = half_size.z;
        glm::vec3 center = this->center();
        corners[0] = center + glm::vec3(-x, -y, -z);
        corners[1] = center + glm::vec3(x, -y, -z);
        corners[2] = center + glm::vec3(x, y, -z);
        corners[3] = center + glm::vec3(-x, y, -z);
        corners[4] = center + glm::vec3(-x, -y, z);
        corners[5] = center + glm::vec3(x, -y, z);
        corners[6] = center + glm::vec3(x, y, z);
        corners[7] = center + glm::vec3(-x, y, z);
        return corners;
    }

    float screen_area(Camera::Ptr camera) const {
        glm::mat4 view_proj =
            camera->getProjectionMatrix() * camera->getViewMatrix();
        auto min = glm::vec2(10000000.0);
        auto max = glm::vec2(-10000000.0);
        auto bb_corners = this->corners();
        for (int i = 0; i < 8; i++) {
            auto clip = view_proj * glm::vec4(bb_corners[i], 1.0f);
            if (clip.w == 0.0f) {
                continue; // skip points that are at infinity
            }
            auto ndc = glm::vec3(clip) / clip.w;

            min = glm::min(min, glm::vec2(ndc.x, ndc.y));
            max = glm::max(max, glm::vec2(ndc.x, ndc.y));
        }
        glm::vec2 size = max - min;
        return size.x * size.y;
    }

    static BB from_splats(SplatVector &splats, bool expand = true) {
        glm::vec3 min = glm::vec3(0.0f);
        glm::vec3 max = glm::vec3(0.0f);

        for (auto &splat : splats) {
            auto position = glm::vec3(splat.transform[3]);
            min.x = std::min(min.x, position.x);
            min.y = std::min(min.y, position.y);
            min.z = std::min(min.z, position.z);

            max.x = std::max(max.x, position.x);
            max.y = std::max(max.y, position.y);
            max.z = std::max(max.z, position.z);
        }

        glm::vec3 center = (max + min) / 2.0f;

        // expand around the center to a cube
        if (!expand) {
            return from_aabb(min, max);
        }
        
        float maxSize = std::max({max.x - min.x, max.y - min.y, max.z - min.z});
        min = center - glm::vec3(maxSize / 2.0f);
        max = center + glm::vec3(maxSize / 2.0f);

        return from_aabb(min, max);
    }
};