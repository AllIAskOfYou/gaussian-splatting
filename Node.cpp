#include "Node.h"
#include <iostream>
#include <glm/gtx/io.hpp>

void Node::addChild(std::shared_ptr<Node> child) {
    children.push_back(child);
    child->updateWorldMatrix(worldMatrix);
}

void Node::updateWorldMatrix(const glm::mat4& parentWorldMatrix) {
    updateLocalMatrix();
    worldMatrix = parentWorldMatrix * localMatrix;
    for (std::shared_ptr<Node> child : children) {
        child->updateWorldMatrix(worldMatrix);
    }
}

void Node::updateLocalMatrix() {
    localMatrix = glm::mat4(1.0f);
    localMatrix = glm::scale(localMatrix, scale);
    localMatrix = glm::mat4(rotation) * localMatrix;
    localMatrix[3] = glm::vec4(position, 1.0f);
}

void Node::rotate(glm::vec3 axis, float angle) {
    rotation = glm::mat3_cast(glm::angleAxis(angle, axis)) * rotation;
}