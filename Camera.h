#pragma once

#include "Node.h"

#include <glm/glm.hpp>
#include <glm/ext.hpp>

// extend the Node class
class Camera : public Node {
public:
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

    // Get the ray dirrection based on screen coordinates
    glm::vec3 getRay(double x, double y) {
        glm::vec4 ray_clip = glm::vec4(x, y, -1.0, 1.0);
        glm::vec4 ray_eye = glm::inverse(getProjectionMatrix()) * ray_clip;
        ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0, 0.0);
        glm::vec3 ray_wor = glm::normalize(glm::vec3(glm::inverse(getViewMatrix()) * ray_eye));
        return ray_wor;
        //float focalLength = 1.0f / glm::tan(glm::radians(fov) / 2.0f);
        //return worldMatrix * glm::vec4(x, y, -focalLength, 0.0f);
        //return -worldMatrix[2];
    }
};