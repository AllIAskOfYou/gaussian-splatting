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
    //std::cout << "Updating world matrix" << std::endl;
    //std::cout << "World matrix: " << worldMatrix << std::endl;
    for (std::shared_ptr<Node> child : children) {
        //std::cout << "Updating child" << std::endl;
        child->updateWorldMatrix(worldMatrix);
    }
}

void Node::updateLocalMatrix() {
    localMatrix = glm::mat4(1.0f);
    localMatrix = glm::scale(localMatrix, scale);
    localMatrix = glm::mat4(rotation) * localMatrix;
    localMatrix[3] = glm::vec4(position, 1.0f);
    //localMatrix = glm::translate(localMatrix, position);
}

void Node::rotate(glm::vec3 axis, float angle) {
    rotation = glm::mat3_cast(glm::angleAxis(angle, axis)) * rotation;
}