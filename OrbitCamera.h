#pragma once

#include "Node.h"
#include "Camera.h"
#include "memory"
#include <glm/glm.hpp>

class OrbitCamera : public Node
{
private:
    double p_xpos;
    double p_ypos;
    bool pressed;

public:
    std::shared_ptr<Camera> camera = std::make_shared<Camera>();

    double scrollRate =1;
    double minDistance = 0.1;

    double orbitRate = 0.5;
    double maxOrbitRate = 1;

    OrbitCamera() : Node(), pressed(false) {
        children.push_back(camera);
    }

    OrbitCamera(float distance) : OrbitCamera() {
        camera->position = glm::vec3(0.0f, 0.0f, distance);
    }

    void onScroll(double xoffset, double yoffset, double deltaTime) {
        camera->position.z += scrollRate * yoffset * (-1) * deltaTime;
        if (camera->position.z < minDistance) {
            camera->position.z = minDistance;
        }
    }

    void onMouseButton(int button, int action, int mods) {
        if (button == 0) {
            pressed = action == 1;
        }
    }

    void onMouseMove(double xpos, double ypos, double deltaTime) {
        orbit(xpos, ypos, deltaTime);
        p_xpos = xpos;
        p_ypos = ypos;
    }

    void orbit(double xpos, double ypos, double deltaTime) {
        if (pressed) {
            double dx = xpos - p_xpos;
            double dy = ypos - p_ypos;

            double rate = orbitRate * deltaTime;
            dx = std::min(maxOrbitRate, std::max(-maxOrbitRate, dx * rate));
            dy = std::min(maxOrbitRate, std::max(-maxOrbitRate, dy * rate));

            rotate(glm::vec3(0.0f, 1.0f, 0.0f), -dx);
            rotate(rotation[0], -dy);

            glm::vec3 cameraForward = rotation * glm::vec3(0.0f, 0.0f, -1.0f);

            glm::vec3 cameraRight = rotation * glm::vec3(1.0f, 0.0f, 0.0f);
            glm::vec3 forward = glm::cross(cameraRight, glm::vec3(0.0f, 1.0f, 0.0f));

            bool sign = glm::dot(forward, cameraForward) > 0;

            if (sign) {
                rotate(rotation[0], dy);
            }
        }
    }
};