#pragma once

#include "Node.h"

#include <glm/glm.hpp>
#include <glm/ext.hpp>

// extend the Node class
class Camera : public Node {
public:
    using Ptr = std::shared_ptr<Camera>;
    // Camera properties
    float fov;
    float aspect;
    float near;
    float far;

    // Default constructor
    Camera() :
        Node(),
        fov(45.0f),
        aspect(1.0f),
        near(0.1f),
        far(100.0f) {}

    // Get the view matrix
    glm::mat4 getViewMatrix() {
        return glm::inverse(worldMatrix);
    }

    // Get the projection matrix
    glm::mat4 getProjectionMatrix() {
        return glm::perspective(glm::radians(fov), aspect, near, far);
    }
};