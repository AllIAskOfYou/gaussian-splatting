#pragma once

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <vector>
#include <memory>

class Node {
public:
    std::vector<std::shared_ptr<Node>> children;

    // Transform
    glm::vec3 position;
    glm::vec3 scale;
    glm::mat3 rotation;

    glm::mat4 localMatrix;
    glm::mat4 worldMatrix;

    // Rendering properties
    // ...

    // Default constructor
    Node() :
        position(0.0f),
        scale(1.0f),
        rotation(1.0f),
        localMatrix(1.0f),
        worldMatrix(1.0f) {}

public:
    void addChild(std::shared_ptr<Node> child);
    void rotate(glm::vec3 axis, float angle);
    void updateWorldMatrix(const glm::mat4& parentWorldMatrix);
    void updateLocalMatrix();
};